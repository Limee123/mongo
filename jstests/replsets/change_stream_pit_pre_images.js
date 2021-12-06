/**
 * Tests that change stream point-in-time pre-images are replicated to secondaries, recovered during
 * startup recovery, and not written during the logical initial sync of a node.
 *
 * The test relies on a correct change stream pre-image recording on a node in the primary role.
 *
 * @tags: [
 * # Change streams are only supported on WiredTiger.
 * requires_wiredtiger,
 * requires_fcv_52,
 * featureFlagChangeStreamPreAndPostImages,
 * ]
 */
(function() {
"use strict";
load("jstests/libs/fail_point_util.js");
load("jstests/libs/retryable_writes_util.js");

const testName = jsTestName();
const preImagesCollectionDatabase = "config";
const preImagesCollectionName = "system.preimages";
const replTest = new ReplSetTest({
    name: testName,
    nodes: [{}, {rsConfig: {priority: 0}}],
    nodeOptions:
        {setParameter: {logComponentVerbosity: tojsononeline({replication: {initialSync: 5}})}}
});
replTest.startSet();
replTest.initiate();

// Returns documents from the pre-images collection from 'node' ordered by _id.ts, _id.applyOpsIndex
// ascending.
function getPreImages(node) {
    return node.getDB(preImagesCollectionDatabase)[preImagesCollectionName]
        .find()
        .sort({"_id.ts": 1, "_id.applyOpsIndex": 1})
        .toArray();
}

// Asserts that documents in the pre-images collection on the primary node are the same as on a
// secondary node.
function assertPreImagesCollectionOnPrimaryMatchesSecondary() {
    assert.docEq(getPreImages(replTest.getPrimary()),
                 getPreImages(replTest.getSecondary()),
                 "pre-images collection content differs");
}

for (const [collectionName, collectionOptions] of [
         ["collStandard", {}],
         ["collClustered", {clusteredIndex: {key: {_id: 1}, unique: true}}]]) {
    const testDB = replTest.getPrimary().getDB(testName);
    jsTest.log(`Testing on collection '${collectionName}'`);

    // Create a collection with change stream pre- and post-images enabled.
    assert.commandWorked(testDB.createCollection(
        collectionName,
        Object.assign({changeStreamPreAndPostImages: {enabled: true}}, collectionOptions)));
    const coll = testDB[collectionName];

    function issueRetryableFindAndModifyCommands(testDB) {
        if (!RetryableWritesUtil.storageEngineSupportsRetryableWrites(
                jsTest.options().storageEngine)) {
            jsTestLog(
                "Retryable writes are not supported, skipping retryable findAndModify testing");
            return;
        }

        // Open a new session with retryable writes set to on.
        const session = testDB.getMongo().startSession({retryWrites: true});
        const coll = session.getDatabase(testName)[collectionName];
        assert.commandWorked(coll.insert({_id: 5, v: 1}));

        // Issue "findAndModify" command to return a document version before update.
        assert.docEq(coll.findAndModify({query: {_id: 5}, update: {$inc: {v: 1}}, new: false}),
                     {_id: 5, v: 1});

        // Issue "findAndModify" command to return a document version after update.
        assert.docEq(coll.findAndModify({query: {_id: 5}, update: {$inc: {v: 1}}, new: true}),
                     {_id: 5, v: 3});

        // Issue "findAndModify" command to return a document version before deletion.
        assert.docEq(
            coll.findAndModify({query: {_id: 5}, new: false, remove: true, writeConcern: {w: 2}}),
            {_id: 5, v: 3});
    }

    (function testSteadyStateReplication() {
        // Insert a document.
        assert.commandWorked(coll.insert({_id: 1, v: 1, largeField: "AAAAAAAAAAAAAAAAAAAAAAAA"}));

        // Update the document by issuing a basic "update" command.
        assert.commandWorked(coll.updateOne({_id: 1}, {$inc: {v: 1}}));

        // Verify that a related change stream pre-image was replicated to the secondary.
        replTest.awaitReplication();
        assertPreImagesCollectionOnPrimaryMatchesSecondary();

        // Issue a "delete" command.
        assert.commandWorked(coll.insert({_id: 2, field: "A"}));
        assert.commandWorked(coll.deleteOne({_id: 2}));

        // Verify that a related change stream pre-image was replicated to the secondary.
        replTest.awaitReplication();
        assertPreImagesCollectionOnPrimaryMatchesSecondary();

        // Issue retryable "findAndModify" commands.
        issueRetryableFindAndModifyCommands(testDB);

        // Verify that a related change stream pre-images were replicated to the secondary.
        replTest.awaitReplication();
        assertPreImagesCollectionOnPrimaryMatchesSecondary();
    })();

    (function testInitialSync() {
        // Insert a document for deletion test.
        assert.commandWorked(coll.insert({_id: 3, field: "A"}, {writeConcern: {w: 2}}));

        // Add a new node that will perform an initial sync. Pause the initial sync process (using
        // failpoint "initialSyncHangBeforeCopyingDatabases") before copying the database to perform
        // document modifications to make the collection content more recent and create inconsistent
        // data situation during oplog application.
        const initialSyncNode = replTest.add({
            rsConfig: {priority: 0},
            setParameter:
                {'failpoint.initialSyncHangBeforeCopyingDatabases': tojson({mode: 'alwaysOn'})}
        });

        // Wait until the new node starts and pauses on the fail point.
        replTest.reInitiate();
        assert.commandWorked(initialSyncNode.adminCommand({
            waitForFailPoint: "initialSyncHangBeforeCopyingDatabases",
            timesEntered: 1,
            maxTimeMS: 60000
        }));

        // Update the document on the primary node.
        assert.commandWorked(coll.updateOne({_id: 1}, {$inc: {v: 1}}, {writeConcern: {w: 2}}));

        // Delete the document on the primary node.
        assert.commandWorked(coll.deleteOne({_id: 3}, {writeConcern: {w: 2}}));

        // Issue retryable "findAndModify" commands.
        issueRetryableFindAndModifyCommands(testDB);

        // Resume the initial sync process.
        assert.commandWorked(initialSyncNode.adminCommand(
            {configureFailPoint: "initialSyncHangBeforeCopyingDatabases", mode: "off"}));

        // Wait until the initial sync process is complete and the new node becomes a fully
        // functioning secondary.
        replTest.waitForState(initialSyncNode, ReplSetTest.State.SECONDARY);

        // Verify that pre-images were not written during the logical initial sync.
        let preImageDocuments = getPreImages(initialSyncNode);
        assert.eq(preImageDocuments.length, 0, preImageDocuments);

        // Verify that in the secondary mode, after initial sync is complete, the pre-images are
        // written.
        assert.commandWorked(coll.updateOne({_id: 1}, {$inc: {v: 1}}, {writeConcern: {w: 3}}));
        preImageDocuments = getPreImages(initialSyncNode);
        assert.eq(preImageDocuments.length, 1, preImageDocuments);
        const preImageDocumentsOnPrimary = getPreImages(replTest.getPrimary());
        assert.docEq(preImageDocuments[0], preImageDocumentsOnPrimary.pop());

        // Remove the initial sync node from the replica set.
        replTest.remove(initialSyncNode);
    })();

    (function testStartupRecovery() {
        // Pause check-pointing on the primary node to ensure new pre-images are not flushed to the
        // disk.
        const pauseCheckpointThreadFailPoint =
            configureFailPoint(replTest.getPrimary(), "pauseCheckpointThread");
        pauseCheckpointThreadFailPoint.wait();

        // Update the document on the primary node.
        assert.commandWorked(coll.updateOne({_id: 1}, {$inc: {v: 1}}, {writeConcern: {w: 2}}));

        // Insert and delete a document on the primary node.
        assert.commandWorked(coll.insert({_id: 4, field: "A"}));
        assert.commandWorked(coll.deleteOne({_id: 4}, {writeConcern: {w: 2}}));

        // Issue retryable "findAndModify" commands.
        issueRetryableFindAndModifyCommands(testDB);

        // Do an unclean shutdown of the primary node, and then restart.
        replTest.stop(0, 9, {allowedExitCode: MongoRunner.EXIT_SIGKILL});
        replTest.restart(0);

        // Wait until the primary is up and running.
        replTest.waitForPrimary();

        // Verify that pre-image documents are the same on the recovered primary and the secondary.
        assertPreImagesCollectionOnPrimaryMatchesSecondary();
    })();
}

replTest.stopSet();
})();
