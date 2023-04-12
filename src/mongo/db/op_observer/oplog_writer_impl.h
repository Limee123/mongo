/**
 *    Copyright (C) 2022-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include "mongo/db/op_observer/oplog_writer.h"

namespace mongo {

class OplogWriterImpl : public OplogWriter {
    OplogWriterImpl(const OplogWriterImpl&) = delete;
    OplogWriterImpl& operator=(const OplogWriterImpl&) = delete;

public:
    OplogWriterImpl() = default;
    virtual ~OplogWriterImpl() = default;

    void appendOplogEntryChainInfo(OperationContext* opCtx,
                                   repl::MutableOplogEntry* oplogEntry,
                                   repl::OplogLink* oplogLink,
                                   const std::vector<StmtId>& stmtIds) override;

    std::vector<repl::OpTime> logInsertOps(OperationContext* opCtx,
                                           repl::MutableOplogEntry* oplogEntryTemplate,
                                           std::vector<InsertStatement>::const_iterator begin,
                                           std::vector<InsertStatement>::const_iterator end,
                                           std::vector<bool> fromMigrate,
                                           const ShardingWriteRouter& shardingWriteRouter,
                                           const CollectionPtr& collectionPtr) override;

    repl::OpTime logOp(OperationContext* opCtx, repl::MutableOplogEntry* oplogEntry) override;

    std::vector<OplogSlot> getNextOpTimes(OperationContext* opCtx, std::size_t count) override;
};

}  // namespace mongo
