/*
 * Copyright Redis Ltd. 2020 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#include "redisraft.h"
#include <stdbool.h>
#include <string.h>
#include <libnetrixclient/netrix.h>
#include <json-c/json.h>

RRStatus NetrixInit(RedisRaftCtx* rr, RedisRaftConfig* rc) {
    NetrixWrapper* wrapper = malloc(sizeof(NetrixWrapper));
    netrix_client_config config;
    
    char* id = malloc(sizeof(char)*3);
    sprintf(id, "%d", (int )rc->id);
    char* listen_addr = malloc(sizeof(char)*300);
    char* netrix_addr = malloc(sizeof(char)*300);
    sprintf(listen_addr, "%s:%u", rc->netrix_listener_addr.host, rc->netrix_listener_addr.port);
    sprintf(netrix_addr, "%s:%u", rc->netrix_server_addr.host, rc->netrix_server_addr.port);

    config.id = id;
    config.info = NULL;
    config.listen_addr = listen_addr;
    config.netrix_addr = netrix_addr;
    
    wrapper->client = netrix_create_client(config);
    wrapper->user_data = rr;
    wrapper->signal = 0;
    wrapper->message_polling_thread = NULL;

    rr->netrix_wrapper = wrapper;
    return RR_OK;
}

int serializeAEReq(raft_appendentries_req_t *msg, char **out_s) {
    json_object* out = json_object_new_object();

    json_object_object_add(out, "leader_id", json_object_new_int((int) msg->leader_id));
    json_object_object_add(out, "term", json_object_new_double((double) msg->term));
    json_object_object_add(out, "prev_log_idx", json_object_new_double((double) msg->prev_log_idx));
    json_object_object_add(out, "prev_log_term", json_object_new_double((double) msg->prev_log_term));
    json_object_object_add(out, "leader_commit", json_object_new_double((double) msg->leader_commit));
    json_object_object_add(out, "msg_id", json_object_new_double((double) msg->msg_id));

    json_object* entries = json_object_new_array_ext(msg->n_entries);
    for(int i = 0; i < msg->n_entries; i++) {
        raft_entry_t *e = msg->entries[i];
        json_object *entry = json_object_new_object();
        json_object_object_add(entry, "term", json_object_new_double((double) e->term));
        json_object_object_add(entry, "id", json_object_new_int((int) e->id));
        json_object_object_add(entry, "session", json_object_new_double((double) e->session));
        json_object_object_add(entry, "type", json_object_new_int((int) e->type));
        json_object_object_add(entry, "data", json_object_new_string_len(e->data, e->data_len));
        json_object_array_put_idx(entries, i, entry);
    }
    json_object_object_add(out, "entries", entries);

    char* result = strdup(json_object_to_json_string(out));
    *out_s = result;
    json_object_put(out);

    return 0;
}

int deserializeAEReq(char *msg, raft_appendentries_req_t *out) {
    json_object* obj = json_tokener_parse(msg);
    if(obj == NULL) {
        return -1;
    }
    json_object* j_leader_id = json_object_object_get(obj, "leader_id");
    json_object* j_term = json_object_object_get(obj, "term");
    json_object* j_prev_log_idx = json_object_object_get(obj, "prev_log_idx");
    json_object* j_prev_log_term = json_object_object_get(obj, "prev_log_term");
    json_object* j_leader_commit = json_object_object_get(obj, "leader_commit");
    json_object* j_msg_id = json_object_object_get(obj, "msg_id");
    json_object* j_entries = json_object_object_get(obj, "entries");

    out->leader_id = (raft_node_id_t) json_object_get_int(j_leader_id);
    out->term = (raft_term_t) json_object_get_double(j_term);
    out->prev_log_idx = (raft_index_t) json_object_get_double(j_prev_log_idx);
    out->prev_log_term = (raft_term_t) json_object_get_double(j_prev_log_term);
    out->leader_commit = (raft_index_t) json_object_get_double(j_leader_commit);
    out->msg_id = (raft_msg_id_t) json_object_get_double(j_msg_id);

    int n_entries = (int) json_object_array_length(j_entries);
    out->n_entries = n_entries;
    out->entries = RedisModule_Calloc(n_entries, sizeof(raft_entry_t));
    for(int i=0; i < n_entries; i++) {
        json_object* j_entry = json_object_array_get_idx(j_entries, i);
        json_object* j_data = json_object_object_get(j_entry, "data");
        int data_len = json_object_get_string_len(j_data);
        json_object* j_term = json_object_object_get(j_entry, "term");
        json_object* j_id = json_object_object_get(j_entry, "id");
        json_object* j_session = json_object_object_get(j_entry, "session");
        json_object* j_type = json_object_object_get(j_entry, "type");

        raft_entry_t *new_entry = raft_entry_new(data_len);
        new_entry->data_len = data_len;
        char *data = strdup(json_object_get_string(j_data));
        strncpy(new_entry->data, data, data_len);
        new_entry->term = (raft_term_t) json_object_get_double(j_term);
        new_entry->id = (raft_entry_id_t) json_object_get_int(j_id);
        new_entry->session = (raft_session_t) json_object_get_double(j_session);
        new_entry->type = json_object_get_int(j_type);
        out->entries[i] = new_entry;
    }

    json_object_put(obj);
    return 0;
}


int serializeAEResp(raft_appendentries_resp_t *msg, char **out) {
    return 0;
}

int deserializeAEResp(char *msg, raft_appendentries_resp_t *out) {
    return 0;
}


int serializeRVReq(raft_requestvote_req_t *msg, char **out) {
    return 0;
}

int deserializeRVReq(char *msg, raft_requestvote_req_t *out) {
    return 0;
}


int serializeRVResp(raft_requestvote_resp_t *msg, char **out) {
    return 0;
}

int deserializeRVResp(char *msg, raft_requestvote_resp_t *out) {
    return 0;
}

int netrixSendAppendEntries(RedisRaftCtx* rr, raft_appendentries_req_t *msg, raft_node_id_t to_id) {
    NetrixWrapper *netrix_wrapper = rr->netrix_wrapper;
    char *req;
    if(serializeAEReq(msg, &req) != 0) {
        return -1;
    }

    char *message_type = "append_entries_request";
    char *to = malloc(sizeof(char)*3);
    sprintf(to, "%d", (int) to_id);

    netrix_message* n_message = netrix_create_message(to, req, message_type);

    netrix_client* client = netrix_wrapper->client;

    int out = 0;
    if (netrix_send_message(client, n_message) != 0) {
        out = -1;
    }
    netrix_free_message(n_message);
    return out;
}

int netrixSendAppendEntriesResponse(RedisRaftCtx* rr, raft_appendentries_resp_t *msg, raft_node_id_t to_id) {
    NetrixWrapper *netrix_wrapper = rr->netrix_wrapper;
    char *req;
    if(serializeAEResp(msg, &req) != 0) {
        return -1;
    }

    char *message_type = "append_entries_response";
    char *to = malloc(sizeof(char)*3);
    sprintf(to, "%d", (int) to_id);

    netrix_message* n_message = netrix_create_message(to, req, message_type);

    netrix_client* client = netrix_wrapper->client;

    int out = 0;
    if (netrix_send_message(client, n_message) != 0) {
        out = -1;
    }
    netrix_free_message(n_message);
    return out;
}

int netrixSendRequestVote(RedisRaftCtx* rr, raft_requestvote_req_t *msg, raft_node_id_t to_id) {
    NetrixWrapper *netrix_wrapper = rr->netrix_wrapper;
    char *req;
    if(serializeRVReq(msg, &req) != 0) {
        return -1;
    }

    char *message_type = "request_vote_request";
    char *to = malloc(sizeof(char)*3);
    sprintf(to, "%d", (int) to_id);

    netrix_message* n_message = netrix_create_message(to, req, message_type);

    netrix_client* client = netrix_wrapper->client;

    int out = 0;
    if (netrix_send_message(client, n_message) != 0) {
        out = -1;
    }
    netrix_free_message(n_message);
    return out;
}

int netrixSendRequestVoteResponse(RedisRaftCtx* rr, raft_requestvote_resp_t *msg, raft_node_id_t to_id) {
    NetrixWrapper *netrix_wrapper = rr->netrix_wrapper;
    char *req;
    if(serializeRVResp(msg, &req) != 0) {
        return -1;
    }

    char *message_type = "request_vote_response";
    char *to = malloc(sizeof(char)*3);
    sprintf(to, "%d", (int) to_id);

    netrix_message* n_message = netrix_create_message(to, req, message_type);

    netrix_client* client = netrix_wrapper->client;

    int out = 0;
    if (netrix_send_message(client, n_message) != 0) {
        out = -1;
    }
    netrix_free_message(n_message);
    return out;
}

int handleNetrixMessage(NetrixWrapper* wrapper, netrix_message* message, void* user_data) {
    int from_id;
    if(sscanf(message->from, "%d", &from_id) != 1) {
        return -1;
    }

    RedisRaftCtx *rr = (RedisRaftCtx*) user_data;

    // Check if redis state is okay to receive messages
    switch (rr->state) {
        case REDIS_RAFT_UNINITIALIZED:
        case REDIS_RAFT_JOINING:
        case REDIS_RAFT_LOADING:
            return -1;
        case REDIS_RAFT_UP:
            break;
    }

    raft_server_t *me = rr->raft;
    raft_node_t *node = raft_get_node(me, (raft_node_id_t) from_id);

    if (strcmp(message->type, "append_entries_request")) {
        raft_appendentries_req_t req;
        if(deserializeAEReq(message->data, &req) == 0) {
            raft_appendentries_resp_t resp = {0};
            if(raft_recv_appendentries(me, node, &req, &resp) == 0) {
                return netrixSendAppendEntriesResponse(rr, &resp, (raft_node_id_t) from_id);
            }
        }
    } else if(strcmp(message->type, "append_entries_response")) {
        raft_appendentries_resp_t req;
        if(deserializeAEResp(message->data, &req) == 0) {
            return raft_recv_appendentries_response(me, node, &req);
        }
    } else if(strcmp(message->type, "request_vote_request")) {
        raft_requestvote_req_t req;
        if(deserializeRVReq(message->data, &req) == 0) {
            raft_requestvote_resp_t resp = {0};
            if(raft_recv_requestvote(me, node, &req, &resp) == 0) {
                return netrixSendRequestVoteResponse(rr, &resp, (raft_node_id_t) from_id);
            }
        }
    } else if(strcmp(message->type, "request_vote_response")) {
        raft_requestvote_resp_t req;
        if(deserializeRVResp(message->data, &req) == 0) {
            return raft_recv_requestvote_response(me, node, &req);
        }
    }
    return 0;
}

void* poll_netrix_messages(void *arg) {
    NetrixWrapper *n_wrapper = (NetrixWrapper*) arg;
    netrix_client *n_client = n_wrapper->client;
    while(n_wrapper->signal == 0) {
        if (netrix_have_message(n_client)) {
            netrix_message *message = netrix_receive_message(n_client);
            if(message != NULL) {
                handleNetrixMessage(n_wrapper, message, n_wrapper->user_data);
                // TODO need to free allocated memory
            }
        }
    }
    return NULL;
}

int NetrixRunClient(RedisRaftCtx* rr) {
    // Start the server and run a thread to read messages
    NetrixWrapper *n_wrapper = rr->netrix_wrapper;
    netrix_client *n_client = n_wrapper->client;

    int ok = netrix_run_client(n_client);
    if(ok != 0) {
        return ok;
    }

    return pthread_create(&n_wrapper->message_polling_thread, NULL, poll_netrix_messages, n_wrapper);
}

int NetrixSignalClient(RedisRaftCtx* rr, int signal) {
    NetrixWrapper *n_wrapper = rr->netrix_wrapper;
    netrix_client *n_client = n_wrapper->client;

    netrix_signal_client(n_client, signal);
    n_wrapper->signal = signal;
    return 0;
}