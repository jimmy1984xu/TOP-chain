﻿// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>

#include "xtxexecutor/xtransaction_executor.h"
#include "xtxexecutor/xtransaction_context.h"
#include "xdata/xgenesis_data.h"

NS_BEG2(top, txexecutor)

REG_XMODULE_LOG(chainbase::enum_xmodule_type::xmodule_type_xtxexecutor, xunit_error_to_string, xconsensus_service_error_base+1, xconsensus_service_error_max);

int32_t xtransaction_executor::exec_one_tx(xaccount_context_t * account_context, const xcons_transaction_ptr_t & tx) {
    if (false == account_context->add_transaction(tx)) {
        xwarn("xtransaction_executor::exec_one_tx fail-add transaction, tx=%s",
            tx->dump().c_str());
        return enum_xtxexecutor_error_tx_nonce_not_match;  // may happen when failure tx is deleted
    }

    size_t before_op_records_size = account_context->get_op_records_size();

    xtransaction_context_t tx_context(account_context, tx);
    int32_t action_ret = tx_context.exec();
    if (action_ret) {
        tx->set_current_exec_status(enum_xunit_tx_exec_status_fail);
        xwarn("xtransaction_executor::exec_one_tx fail-tx exec abnormal, tx=%s,error:%s",
            tx->dump(true).c_str(), chainbase::xmodule_error_to_str(action_ret).c_str());
        return action_ret;
    }

    size_t after_op_records_size = account_context->get_op_records_size();
    if (tx->is_self_tx() || tx->is_send_tx()) {
        if (after_op_records_size == before_op_records_size) {
            tx->set_current_exec_status(enum_xunit_tx_exec_status_fail);
            xassert(data::is_sys_contract_address(common::xaccount_address_t{tx->get_source_addr()}));
            xinfo("xtransaction_executor::exec_one_tx fail-tx exec no property change, tx=%s",
                tx->dump(true).c_str());
            return xunit_contract_exec_no_property_change;
        }
    }

    tx->set_current_exec_status(enum_xunit_tx_exec_status_success);
    xkinfo("xtransaction_executor::exec_one_tx succ, tx=%s,tx_state=%s",
        tx->dump(true).c_str(), tx->dump_execute_state().c_str());
    return xsuccess;
}

int32_t xtransaction_executor::exec_tx(xaccount_context_t * account_context, const xcons_transaction_ptr_t & tx, std::vector<xcons_transaction_ptr_t> & contract_create_txs) {
    int32_t ret = exec_one_tx(account_context, tx);
    if (ret != xsuccess) {
        xwarn("xtransaction_executor::exec_tx input tx fail. %s error:%s",
            tx->dump().c_str(), chainbase::xmodule_error_to_str(ret).c_str());
        return ret;
    }

    auto & create_txs = account_context->get_create_txs();
    // exec txs created by origin tx secondly, this tx must be a run contract transaction
    if (!create_txs.empty()) {
        for (auto & new_tx : create_txs) {
            xinfo("xtransaction_executor::exec_tx contract create tx. account_txnonce=%ld,input_tx:%s new_tx:%s",
                account_context->get_blockchain()->get_latest_send_trans_number(), tx->dump(true).c_str(), new_tx->dump(true).c_str());
            ret = exec_one_tx(account_context, new_tx);
            if (ret != xsuccess && ret != xunit_contract_exec_no_property_change) {  // TODO(jimmy)
                xwarn("xtransaction_executor::exec_tx contract create tx fail. %s error:%s",
                    new_tx->dump().c_str(), chainbase::xmodule_error_to_str(ret).c_str());
                return ret;
            }

            contract_create_txs.push_back(new_tx);
        }
    }

    return xsuccess;
}

int32_t xtransaction_executor::exec_batch_txs(base::xvblock_t* proposal_block,
                                              const xobject_ptr_t<base::xvbstate_t> & prev_bstate,
                                              const data::xblock_consensus_para_t & cs_para,
                                              const std::vector<xcons_transaction_ptr_t> & txs,
                                              store::xstore_face_t* store,
                                              xbatch_txs_result_t & txs_result) {

    std::vector<xcons_transaction_ptr_t> exec_txs = txs;
    std::vector<xcons_transaction_ptr_t> failure_send_txs;
    std::vector<xcons_transaction_ptr_t> failure_receipt_txs;
    std::vector<xcons_transaction_ptr_t> contract_create_txs;
    int32_t error_code = xsuccess;  // record the last failure tx error code
    std::shared_ptr<store::xaccount_context_t> _account_context = nullptr;
    xaccount_ptr_t proposal_state = nullptr;
    xobject_ptr_t<base::xvbstate_t> proposal_bstate = nullptr;
    do {
        // clone new bstate firstly
        proposal_bstate = make_object_ptr<base::xvbstate_t>(*proposal_block, *prev_bstate.get());
        proposal_state = std::make_shared<xunit_bstate_t>(proposal_bstate.get());

        // create tx execute context
        _account_context = std::make_shared<store::xaccount_context_t>(proposal_state, store);
        _account_context->set_context_para(cs_para.get_clock(), cs_para.get_random_seed(), cs_para.get_timestamp(), cs_para.get_total_lock_tgas_token());
        xassert(!cs_para.get_table_account().empty());
        xassert(cs_para.get_table_proposal_height() > 0);
        uint64_t table_committed_height = cs_para.get_table_proposal_height() >= 3 ? cs_para.get_table_proposal_height() - 3 : 0;
        _account_context->set_context_pare_current_table(cs_para.get_table_account(), table_committed_height);

        // clear old contract create txs
        contract_create_txs.clear();

        // try to execute all txs
        bool all_txs_succ = true;
        for (auto iter = exec_txs.begin(); iter != exec_txs.end(); iter++) {
            auto cur_tx = *iter;
            // execute input tx,
            int32_t action_ret = xtransaction_executor::exec_tx(_account_context.get(), cur_tx, contract_create_txs);
            if (action_ret) {
                error_code = action_ret;
                all_txs_succ = false;
                iter = exec_txs.erase(iter);

                if (cur_tx->is_send_tx() || cur_tx->is_self_tx()) {  // erase other send tx for nonce unmatching
                    failure_send_txs.push_back(cur_tx);
                    for (;iter != exec_txs.end();) {
                        auto other_tx = *iter;
                        if ( (other_tx->is_send_tx() || other_tx->is_self_tx()) && (other_tx->get_tx_nonce() >= cur_tx->get_tx_nonce()) ) {
                            failure_send_txs.push_back(other_tx);
                            iter = exec_txs.erase(iter);
                            xdbg("xtransaction_executor::exec_batch_txs drop other sendtxs");
                        } else {
                            break;  // break when no other send txs
                        }
                    }
                } else {
                    failure_receipt_txs.push_back(cur_tx);
                }
                break;  // break exec when one tx exec fail
            }
        }
        if (all_txs_succ == true) {
            break;  // break when all txs succ
        } else {
            if (exec_txs.size() == 0) {
                break; // break when all txs fail
            } else {
                xwarn("xtransaction_executor::exec_batch_txs retry exec again. %s,account=%s,height=%ld,origin_txs=%zu",
                    cs_para.dump().c_str(), proposal_block->get_account().c_str(), proposal_block->get_height(), txs.size());
                continue;  // retry execute left txs again
            }
        }
    } while(1);

    txs_result.m_exec_fail_txs = failure_send_txs;  // failure send txs need delete from txpool

    // merge all pack txs
    std::vector<xcons_transaction_ptr_t> all_pack_txs;
    for (auto & tx : exec_txs) {
        xassert(tx->get_current_exec_status() == enum_xunit_tx_exec_status_success);
        all_pack_txs.push_back(tx);
    }
    for (auto & tx : contract_create_txs) {
        xassert(tx->get_current_exec_status() == enum_xunit_tx_exec_status_success);
        all_pack_txs.push_back(tx);
    }
    for (auto & tx : failure_receipt_txs) {
        xassert(tx->get_current_exec_status() == enum_xunit_tx_exec_status_fail);
        xassert(tx->is_recv_tx());
        all_pack_txs.push_back(tx);
    }

    xinfo("xtransaction_executor::exec_batch_txs %s,account=%s,height=%ld,origin=%zu,succ=%zu,fail_send=%zu,fail_recv=%zu,contract_txs=%zu,all_pack=%zu",
        cs_para.dump().c_str(), proposal_block->get_account().c_str(), proposal_block->get_height(),
        txs.size(), exec_txs.size(), failure_send_txs.size(), failure_receipt_txs.size(), contract_create_txs.size(), all_pack_txs.size());
    if (exec_txs.empty()) {
        xassert(error_code != xsuccess);
        xassert(!txs_result.m_exec_fail_txs.empty());
        return error_code;
    }

    // update tx related propertys and other default propertys
    if (false == _account_context->finish_exec_all_txs(all_pack_txs)) {
        xerror("xtransaction_executor::exec_batch_txs fail-update tx info. %s,account=%s,height=%ld,origin_txs=%zu,all_txs=%zu",
            cs_para.dump().c_str(), proposal_block->get_account().c_str(), proposal_block->get_height(), txs.size(), all_pack_txs.size());
        return -1;
    }

    xtransaction_result_t result;
    _account_context->get_transaction_result(result);

    txs_result.m_exec_succ_txs = all_pack_txs;
    txs_result.m_unconfirm_tx_num = proposal_state->get_unconfirm_sendtx_num();
    txs_result.m_full_state = result.m_full_state;
    txs_result.m_property_binlog = result.m_property_binlog;

    return xsuccess;
}

NS_END2
