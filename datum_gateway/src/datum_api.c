/*
 *
 * DATUM Gateway
 * Decentralized Alternative Templates for Universal Mining
 *
 * This file is part of OCEAN's Bitcoin mining decentralization
 * project, DATUM.
 *
 * https://ocean.xyz
 *
 * ---
 *
 * Copyright (c) 2024 Bitcoin Ocean, LLC & Jason Hughes
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

// This is quick and dirty for now.  Will be improved over time.

#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <inttypes.h>
#include <jansson.h>

#include "datum_conf.h"
#include "datum_utils.h"
#include "datum_stratum.h"
#include "datum_sockets.h"
#include "datum_protocol.h"

// TODO: Not this.
const char *homepage_html_start = "<HTML><HEAD><TITLE>DATUM Gateway</TITLE><style>body { background-color: black; color: white; font-family: Arial, sans-serif; } a { color: lightblue; text-decoration: none; } a:hover { color: red; text-decoration: underline; } a:visited { color: lightblue; } .fixed-width { font-family: \"Courier New\", Courier, monospace; }</style></HEAD><BODY>";
const char *homepage_html_end = "</BODY></HTML>";

#define DATUM_API_HOMEPAGE_MAX_SIZE 128000

#define HPSZREMAIN(i) (((DATUM_API_HOMEPAGE_MAX_SIZE-1-i)>=0)?(DATUM_API_HOMEPAGE_MAX_SIZE-1-i):0)

const char *cbnames[] = {
	"Blank",
	"Tiny",
	"Default",
	"Respect",
	"Yuge",
	"Antmain2"
};

size_t strncpy_html_escape(char *dest, const char *src, size_t n) {
	size_t i = 0;

	while (*src && i < n) {
		switch (*src) {
			case '&':
				if (i + 5 <= n) { // &amp;
					dest[i++] = '&';
					dest[i++] = 'a';
					dest[i++] = 'm';
					dest[i++] = 'p';
					dest[i++] = ';';
				} else {
					return i; // Stop if there's not enough space
				}
				break;
			case '<':
				if (i + 4 <= n) { // &lt;
					dest[i++] = '&';
					dest[i++] = 'l';
					dest[i++] = 't';
					dest[i++] = ';';
				} else {
					return i; // Stop if there's not enough space
				}
				break;
			case '>':
				if (i + 4 <= n) { // &gt;
					dest[i++] = '&';
					dest[i++] = 'g';
					dest[i++] = 't';
					dest[i++] = ';';
				} else {
					return i; // Stop if there's not enough space
				}
				break;
			case '"':
				if (i + 6 <= n) { // &quot;
					dest[i++] = '&';
					dest[i++] = 'q';
					dest[i++] = 'u';
					dest[i++] = 'o';
					dest[i++] = 't';
					dest[i++] = ';';
				} else {
					return i; // Stop if there's not enough space
				}
				break;
			default:
				dest[i++] = *src;
				break;
		}
		src++;
	}

	// Null-terminate the destination string if there's space
	if (i < n) {
		dest[i] = '\0';
	}

	return i;
}

void datum_api_cmd_empty_thread(int tid) {
	if ((tid >= 0) && (tid < global_stratum_app->max_threads)) {
		DLOG_WARN("API Request to empty stratum thread %d!", tid);
		global_stratum_app->datum_threads[tid].empty_request = true;
	}
}

void datum_api_cmd_kill_client(int tid, int cid) {
	if ((tid >= 0) && (tid < global_stratum_app->max_threads)) {
		if ((cid >= 0) && (cid < global_stratum_app->max_clients_thread)) {
			DLOG_WARN("API Request to disconnect stratum client %d/%d!", tid, cid);
			global_stratum_app->datum_threads[tid].client_data[cid].kill_request = true;
			global_stratum_app->datum_threads[tid].has_client_kill_request = true;
		}
	}
}

int datum_api_cmd(struct MHD_Connection *connection, char *post, int len) {

	struct MHD_Response *response;
	char output[1024];
	int ret, sz=0;
	json_t *root, *cmd, *param;
    json_error_t error;
	const char *cstr;
	int tid,cid;
	
	if ((len) && (post)) {
		DLOG_DEBUG("POST DATA: %s", post);
		
		if (post[0] == '{') {
			// attempt to parse JSON command
			root = json_loadb(post, len, 0, &error);
			if (root) {
			
				if (json_is_object(root) && (cmd = json_object_get(root, "cmd"))) {
					if (json_is_string(cmd)) {
						cstr = json_string_value(cmd);
						DLOG_DEBUG("JSON CMD: %s",cstr);
						switch(cstr[0]) {
							case 'e': {
								if (!strcmp(cstr,"empty_thread")) {
									param = json_object_get(root, "tid");
									if (json_is_integer(param)) {
										datum_api_cmd_empty_thread(json_integer_value(param));
									}
									break;
								}
								break;
							}
							case 'k': {
								if (!strcmp(cstr,"kill_client")) {
									param = json_object_get(root, "tid");
									if (json_is_integer(param)) {
										tid = json_integer_value(param);
										param = json_object_get(root, "cid");
										if (json_is_integer(param)) {
											cid = json_integer_value(param);
											datum_api_cmd_kill_client(tid,cid);
										}
									}
									break;
								}
								break;
							}
							default: break;
						}
					}
				}
			
			}
		}
		
	}
	
	sprintf(output, "{}");
	response = MHD_create_response_from_buffer (sz, (void *) output, MHD_RESPMEM_MUST_COPY);
	MHD_add_response_header(response, "Content-Type", "application/json");
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	return ret;

}

int datum_api_thread_dashboard(struct MHD_Connection *connection) {

	struct MHD_Response *response;
	int connected_clients = 0;
	int i,sz=0,ret,max_sz = 0,j,ii;
	char *output = NULL;
	T_DATUM_MINER_DATA *m = NULL;
	uint64_t tsms;
	double hr;
	unsigned char astat;
	double thr = 0.0;
	int subs,conns;
	
	for(i=0;i<global_stratum_app->max_threads;i++) {
		connected_clients+=global_stratum_app->datum_threads[i].connected_clients;
	}
	
	max_sz = strlen(homepage_html_start) + strlen(homepage_html_end) + (connected_clients * 784) + 2048; // approximate max size of each row
	output = calloc(max_sz+16,1);
	if (!output) {
		return MHD_NO;
	}
	
	tsms = current_time_millis();
	
	sz = snprintf(output, max_sz-1-sz, "%s<H1>DATUM Gateway Threads</H1><H2><A HREF=\"/\">Home</A></H2><BR><TABLE BORDER=\"1\">", homepage_html_start);
	sz += snprintf(&output[sz], max_sz-1-sz, "<TR><TD><U>TID</U></TD>  <TD><U>Connection Count</U></TD>  <TD><U>Sub Count</U></TD> <TD><U>Approx. Hashrate</U></TD> <TD><U>Command</U></TD></TR>");

	for(j=0;j<global_stratum_app->max_threads;j++) {
		thr = 0.0;
		subs = 0;
		conns = 0;
		
		for(ii=0;ii<global_stratum_app->max_clients_thread;ii++) {
			if (global_stratum_app->datum_threads[j].client_data[ii].fd > 0) {
				conns++;
				m = (T_DATUM_MINER_DATA *)global_stratum_app->datum_threads[j].client_data[ii].app_client_data;
				if (m->subscribed) {
					subs++;
					astat = m->stats.active_index?0:1; // inverted
					hr = 0.0;
					if ((m->stats.last_swap_ms > 0) && (m->stats.diff_accepted[astat] > 0)) {
						hr = ((double)m->stats.diff_accepted[astat] / (double)((double)m->stats.last_swap_ms/1000.0)) * 0.004294967296; // Th/sec based on shares/sec
					}
					if (((double)(tsms - m->stats.last_swap_tsms)/1000.0) < 180.0) {
						thr += hr;
					}
				}
			}
		}
		if (conns) {
			sz += snprintf(&output[sz], max_sz-1-sz, "<TR><TD>%d</TD>  <TD>%d</TD>  <TD>%d</TD> <TD>%.2f Th/s</TD> <TD><button onclick=\"sendPostRequest('/cmd', {cmd:'empty_thread',tid:%d})\">Disconnect All</button></TD></TR>", j, conns, subs, thr, j);
		}
	}
	sz += snprintf(&output[sz], max_sz-1-sz, "</TABLE>");
	sz += snprintf(&output[sz], max_sz-1-sz, "<script>function sendPostRequest(url, data){fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});}</script>");
	
	response = MHD_create_response_from_buffer (sz, (void *) output, MHD_RESPMEM_MUST_FREE);
	MHD_add_response_header(response, "Content-Type", "text/html");
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	return ret;

}


int datum_api_client_dashboard(struct MHD_Connection *connection) {

	struct MHD_Response *response;
	int connected_clients = 0;
	int i,sz=0,ret,max_sz = 0,j,ii;
	char *output = NULL;
	T_DATUM_MINER_DATA *m = NULL;
	uint64_t tsms;
	double hr;
	unsigned char astat;
	double thr = 0.0;
	
	for(i=0;i<global_stratum_app->max_threads;i++) {
		connected_clients+=global_stratum_app->datum_threads[i].connected_clients;
	}
	
	max_sz = strlen(homepage_html_start) + strlen(homepage_html_end) + (connected_clients * 1024) + 2048; // approximate max size of each row
	output = calloc(max_sz+16,1);
	if (!output) {
		return MHD_NO;
	}
	
	tsms = current_time_millis();
	
	sz = snprintf(output, max_sz-1-sz, "%s<H1>DATUM Gateway Clients</H1><H2><A HREF=\"/\">Home</A></H2><BR><TABLE BORDER=\"1\">", homepage_html_start);
	sz += snprintf(&output[sz], max_sz-1-sz, "<TR><TD><U>TID/CID</U></TD>  <TD><U>RemHost</U></TD>  <TD><U>Auth Username</U></TD> <TD><U>Subbed</U></TD> <TD><U>Last Accepted</U></TD> <TD><U>VDiff</U></TD> <TD><U>DiffA (A)</U></TD> <TD><U>DiffR (R)</U></TD> <TD><U>Hashrate (age)</U></TD> <TD><U>Coinbase</U></TD> <TD><U>UserAgent</U> </TD><TD><U>Command</U></TD></TR>");
	// <TD><U></U></TD>
	for(j=0;j<global_stratum_app->max_threads;j++) {
		for(ii=0;ii<global_stratum_app->max_clients_thread;ii++) {
			if (global_stratum_app->datum_threads[j].client_data[ii].fd > 0) {
				m = (T_DATUM_MINER_DATA *)global_stratum_app->datum_threads[j].client_data[ii].app_client_data;
				sz += snprintf(&output[sz], max_sz-1-sz, "<TR><TD>%d/%d</TD>", j,ii);
				
				sz += snprintf(&output[sz], max_sz-1-sz, "<TD>%s</TD>", global_stratum_app->datum_threads[j].client_data[ii].rem_host);

				
				sz += snprintf(&output[sz], max_sz-1-sz, "<TD>");
				sz += strncpy_html_escape(&output[sz], m->last_auth_username, max_sz-1-sz);
				sz += snprintf(&output[sz], max_sz-1-sz, "</TD>");

				if (m->subscribed) {
					sz += snprintf(&output[sz], max_sz-1-sz, "<TD> <span style=\"font-family: monospace;\">%4.4x</span> %.1fs</TD>", m->sid, (double)(tsms - m->subscribe_tsms)/1000.0);
					
					if (m->stats.last_share_tsms) {
						sz += snprintf(&output[sz], max_sz-1-sz, "<TD>%.1fs</TD>", (double)(tsms - m->stats.last_share_tsms)/1000.0);
					} else {
						sz += snprintf(&output[sz], max_sz-1-sz, "<TD>N/A</TD>");
					}

					sz += snprintf(&output[sz], max_sz-1-sz, "<TD>%"PRIu64"</TD>", m->current_diff);
					sz += snprintf(&output[sz], max_sz-1-sz, "<TD>%"PRIu64" (%"PRIu64")</TD>", m->share_diff_accepted, m->share_count_accepted);
					
					hr = 0.0;
					if (m->share_diff_accepted > 0) {
						hr = ((double)m->share_diff_rejected / (double)(m->share_diff_accepted + m->share_diff_rejected))*100.0;
					}
					sz += snprintf(&output[sz], max_sz-1-sz, "<TD>%"PRIu64" (%"PRIu64") %.2f%%</TD>", m->share_diff_rejected, m->share_count_rejected, hr);



					astat = m->stats.active_index?0:1; // inverted
					hr = 0.0;
					if ((m->stats.last_swap_ms > 0) && (m->stats.diff_accepted[astat] > 0)) {
						hr = ((double)m->stats.diff_accepted[astat] / (double)((double)m->stats.last_swap_ms/1000.0)) * 0.004294967296; // Th/sec based on shares/sec
					}
					if (((double)(tsms - m->stats.last_swap_tsms)/1000.0) < 180.0) {
						thr += hr;
					}
					if (m->share_diff_accepted > 0) {
						sz += snprintf(&output[sz], max_sz-1-sz, "<TD>%.2f Th/s (%.1fs)</TD>", hr, (double)(tsms - m->stats.last_swap_tsms)/1000.0);
					} else {
						sz += snprintf(&output[sz], max_sz-1-sz, "<TD>N/A</TD>");
					}

					if (m->coinbase_selection < (sizeof(cbnames) / sizeof(cbnames[0]))) {
						sz += snprintf(&output[sz], max_sz-1-sz, "<TD>%s</TD>", cbnames[m->coinbase_selection]);
					} else {
						sz += snprintf(&output[sz], max_sz-1-sz, "<TD>Unknown</TD>");
					}


					sz += snprintf(&output[sz], max_sz-1-sz, "<TD>");
					sz += strncpy_html_escape(&output[sz], m->useragent, max_sz-1-sz);
					sz += snprintf(&output[sz], max_sz-1-sz, "</TD>");


				} else {
					sz += snprintf(&output[sz], max_sz-1-sz, "<TD COLSPAN=\"8\">Not Subscribed</TD>");
				}

				

				sz += snprintf(&output[sz], max_sz-1-sz, "<TD><button onclick=\"sendPostRequest('/cmd', {cmd:'kill_client',tid:%d,cid:%d})\">Kick</button></TD></TR>", j, ii);
				
			}
		}
	}
	
	sz += snprintf(&output[sz], max_sz-1-sz, "</TABLE><BR>Total active hashrate estimate: %.2f Th/s", thr);
	sz += snprintf(&output[sz], max_sz-1-sz, "<script>function sendPostRequest(url, data){fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});}</script>");
	
	// return the home page with some data and such
	response = MHD_create_response_from_buffer (sz, (void *) output, MHD_RESPMEM_MUST_FREE);
	MHD_add_response_header(response, "Content-Type", "text/html");
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	return ret;

}


int datum_api_homepage(struct MHD_Connection *connection) {

	struct MHD_Response *response;
	char output[DATUM_API_HOMEPAGE_MAX_SIZE];
	int i,j,k,kk,ii,ret;
	T_DATUM_STRATUM_JOB *sjob;
	T_DATUM_MINER_DATA *m;
	
	// include current stratum job status
	i = snprintf(output, DATUM_API_HOMEPAGE_MAX_SIZE-1, "%s<H1>DATUM Gateway Status</H1>", homepage_html_start);
	pthread_rwlock_rdlock(&stratum_global_job_ptr_lock);
		j = global_latest_stratum_job_index;
		sjob = global_cur_stratum_jobs[j];
	pthread_rwlock_unlock(&stratum_global_job_ptr_lock);

	i += snprintf(&output[i], HPSZREMAIN(i), "<H2><A HREF=\"/clients\">Client List</A> - <A HREF=\"/threads\">Thread List</A></H2><BR>");

	i+= snprintf(&output[i], HPSZREMAIN(i), "<TABLE BORDER=1><TR><TD COLSPAN=\"2\"><B>Decentralized Client Stats</B></TD></TR><TR><TD>Shares Accepted:</TD><TD>%"PRIu64" (%"PRIu64" diff)</TD></TR><TR><TD>Shares Rejected:</TD><TD>%"PRIu64" (%"PRIu64" diff)</TD></TR>",
		datum_accepted_share_count, datum_accepted_share_diff, datum_rejected_share_count, datum_rejected_share_diff);
	i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Status:</TD><TD>%s</TD></TR>",datum_protocol_is_active()?"Connected and Ready":"Not Ready");
	i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Pool Host:</TD><TD>%s:%d</TD></TR>", datum_config.datum_pool_host, datum_config.datum_pool_port);
	i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Pool Tag:</TD><TD>\"%s\"</TD></TR>", datum_config.override_mining_coinbase_tag_primary);
	i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Secondary/Miner Tag:</TD><TD>\"%s\"</TD></TR>", datum_config.mining_coinbase_tag_secondary);
	i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Pool Current MinDiff:</TD><TD>%"PRIu64"</TD></TR>", datum_config.override_vardiff_min);
	i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Pool Pubkey:</TD><TD>%s</TD></TR></TABLE><BR><BR>", datum_config.datum_pool_pubkey);


	if (sjob) {
		i += snprintf(&output[i], HPSZREMAIN(i), "<TABLE BORDER=\"1\"><TR><TD COLSPAN=\"2\"><B>Current Stratum Job</B></TD></TR>");
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Job Index:</TD><TD>%d</TD></TR>", j);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Job ID:</TD><TD class=\"fixed-width\">%s</TD></TR>", sjob->job_id);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Block Height:</TD><TD>%d</TD></TR>", (int)sjob->block_template->height);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Block Value:</TD><TD>%.8f BTC</TD></TR>", (double)sjob->block_template->coinbasevalue / (double)100000000.0);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Previous Block Hash:</TD><TD class=\"fixed-width\">%s</TD></TR>", sjob->block_template->previousblockhash);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Block Target:</TD><TD class=\"fixed-width\">%s</TD></TR>", sjob->block_template->block_target_hex);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Block Difficulty:</TD><TD>%.3Lf</TD></TR>", calc_network_difficulty(sjob->nbits));
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Version:</TD><TD>%s (%u)</TD></TR>", sjob->version, (unsigned)sjob->version_uint);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Bits:</TD><TD>%s (%u)</TD></TR>", sjob->nbits, (unsigned)sjob->nbits_uint);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Time:</TD><TD>%s</TD></TR>", sjob->ntime);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Is New Block:</TD><TD>%s</TD></TR>", sjob->is_new_block ? "Yes" : "No");
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Job Timestamp:</TD><TD>%.3f</TD></TR>", (double)sjob->tsms/1000.0);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Template Local Index:</TD><TD>%u</TD></TR>", (unsigned)sjob->block_template->local_index);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Minimum Time:</TD><TD>%llu</TD></TR>", (unsigned long long)sjob->block_template->mintime);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Current Time:</TD><TD>%llu</TD></TR>", (unsigned long long)sjob->block_template->curtime);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Size Limit:</TD><TD>%llu</TD></TR>", (unsigned long long)sjob->block_template->sizelimit);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Weight Limit:</TD><TD>%llu</TD></TR>", (unsigned long long)sjob->block_template->weightlimit);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>SigOp Limit:</TD><TD>%u</TD></TR>", (unsigned)sjob->block_template->sigoplimit);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Witness Commitment:</TD><TD class=\"fixed-width\">%s</TD></TR>", sjob->block_template->default_witness_commitment);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Transaction Count:</TD><TD>%u</TD></TR>", (unsigned)sjob->block_template->txn_count);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Total Transaction Weight:</TD><TD>%u</TD></TR>", (unsigned)sjob->block_template->txn_total_weight);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Total Transaction Size:</TD><TD>%u</TD></TR>", (unsigned)sjob->block_template->txn_total_size);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Total SigOps:</TD><TD>%u</TD></TR>", (unsigned)sjob->block_template->txn_total_sigops);
		i += snprintf(&output[i], HPSZREMAIN(i), "</TABLE><BR>");
		
		if (sjob->is_new_block) {
			i+=snprintf(&output[i], HPSZREMAIN(i), "COINBASE TXN TYPE E: %sE1E1E1E1E2E2E2E2E2E2E2E2%s<BR>", sjob->subsidy_only_coinbase.coinb1, sjob->subsidy_only_coinbase.coinb2);
		} else {
			kk = 0;
			i+=snprintf(&output[i], HPSZREMAIN(i), "COINBASE TXN TYPE %d: %sE1E1E1E1E2E2E2E2E2E2E2E2%s<BR>", kk, sjob->coinbase[kk].coinb1, sjob->coinbase[kk].coinb2);
		}
		kk = 2;
		i+=snprintf(&output[i], HPSZREMAIN(i), "COINBASE TXN TYPE %d: %sE1E1E1E1E2E2E2E2E2E2E2E2%s<BR>", kk, sjob->coinbase[kk].coinb1, sjob->coinbase[kk].coinb2);
		kk = 5;
		i+=snprintf(&output[i], HPSZREMAIN(i), "COINBASE TXN TYPE %d: %sE1E1E1E1E2E2E2E2E2E2E2E2%s<BR>", kk, sjob->coinbase[kk].coinb1, sjob->coinbase[kk].coinb2);
		
	}

	if (global_stratum_app) {
		i += snprintf(&output[i], HPSZREMAIN(i), "<TABLE BORDER=\"1\"><TR><TD COLSPAN=\"2\"><B>Stratum Server Info</B></TD></TR>");
		
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Active threads:</TD><TD>%d</TD></TR>", global_stratum_app->datum_active_threads);
		
		k = 0;
		kk = 0;
		for(j=0;j<global_stratum_app->max_threads;j++) {
			k+=global_stratum_app->datum_threads[j].connected_clients;
			for(ii=0;ii<global_stratum_app->max_clients_thread;ii++) {
				if (global_stratum_app->datum_threads[j].client_data[ii].fd > 0) {
					m = (T_DATUM_MINER_DATA *)global_stratum_app->datum_threads[j].client_data[ii].app_client_data;
					if (m->subscribed) kk++;
				}
			}
		}
		
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Total connections:</TD><TD>%d</TD></TR>", k);
		i += snprintf(&output[i], HPSZREMAIN(i), "<TR><TD>Total work subscriptions:</TD><TD>%d</TD></TR>", kk);
		
		
		i += snprintf(&output[i], HPSZREMAIN(i), "</TABLE><BR>");
	}
	
	// end HTML
	i += snprintf(&output[i], HPSZREMAIN(i), "%s", homepage_html_end);
	
	// return the home page with some data and such
	response = MHD_create_response_from_buffer (i, (void *) output, MHD_RESPMEM_MUST_COPY);
	MHD_add_response_header(response, "Content-Type", "text/html");
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	return ret;


}

int datum_api_OK(struct MHD_Connection *connection) {
	enum MHD_Result ret;
	struct MHD_Response *response;
	const char *ok_response = "OK";
	response = MHD_create_response_from_buffer(strlen(ok_response), (void *)ok_response, MHD_RESPMEM_PERSISTENT);
	MHD_add_response_header(response, "Content-Type", "text/html");
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	return ret;
}

struct ConnectionInfo {
	char *data;
	size_t data_size;
};

static void datum_api_request_completed(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
	struct ConnectionInfo *con_info = *con_cls;

	if (con_info != NULL) {
		if (con_info->data != NULL) free(con_info->data);
		free(con_info);
	}
	*con_cls = NULL;
}

enum MHD_Result datum_api_answer(void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {
	char *user;
	char *pass;
	enum MHD_Result ret;
	struct MHD_Response *response;
	struct ConnectionInfo *con_info = *con_cls;
	int int_method = 0;
	int uds = 0;
	const char *time_str;
	
    if (strcmp(method, "GET") == 0) {
		int_method = 1;
    }
	
    if (strcmp(method, "POST") == 0) {
		int_method = 2;
    }
	
	if (!int_method) {
        const char *error_response = "<H1>Method not allowed.</H1>";
        response = MHD_create_response_from_buffer(strlen(error_response), (void *)error_response, MHD_RESPMEM_PERSISTENT);
		MHD_add_response_header(response, "Content-Type", "text/html");
        ret = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response);
        MHD_destroy_response(response);
        return ret;
	}
	
	if (int_method == 2) {
		if (!con_info) {
			// Allocate memory for connection info
			con_info = malloc(sizeof(struct ConnectionInfo));
			if (!con_info)
				return MHD_NO;

			con_info->data = calloc(16384,1);
			con_info->data_size = 0;

			if (!con_info->data) {
				free(con_info);
				return MHD_NO;
			}
		
			*con_cls = (void *)con_info;
			
			return MHD_YES;
		}
		
		if (*upload_data_size) {
			// Accumulate data
			
			// max 1 MB? seems reasonable
			if (con_info->data_size + *upload_data_size > (1024*1024)) return MHD_NO;
			
			con_info->data = realloc(con_info->data, con_info->data_size + *upload_data_size + 1);
			if (!con_info->data) {
				return MHD_NO;
			}
			memcpy(&(con_info->data[con_info->data_size]), upload_data, *upload_data_size);
			con_info->data_size += *upload_data_size;
			con_info->data[con_info->data_size] = '\0';
			*upload_data_size = 0;

			return MHD_YES;
		} else if (!con_info->data_size) {
			const char *error_response = "<H1>Invalid request.</H1>";
			response = MHD_create_response_from_buffer(strlen(error_response), (void *)error_response, MHD_RESPMEM_PERSISTENT);
			MHD_add_response_header(response, "Content-Type", "text/html");
			ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
			MHD_destroy_response(response);
			return ret;
		}
		
		uds = *upload_data_size;
		
	}
	
	const union MHD_ConnectionInfo *conn_info = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
	char *client_ip = inet_ntoa(((struct sockaddr_in*)conn_info->client_addr)->sin_addr);

	DLOG_DEBUG("REQUEST: %s, %s, %s, %d", client_ip, method, url, uds);
	
	pass = NULL;
	user = MHD_basic_auth_get_username_password (connection, &pass);

	/////////////////////////
	// TODO: Implement API key or auth or something similar

	if (NULL != user)
		MHD_free (user);
	if (NULL != pass)
		MHD_free (pass);
	
	
	if (int_method == 1 && url[0] == '/' && url[1] == 0) {
		// homepage
		return datum_api_homepage(connection);
	}
	
	switch (url[1]) {
	
		case 'N': {
			if (!strcmp(url, "/NOTIFY")) {
				// TODO: Implement faster notifies with hash+height
				datum_blocktemplates_notifynew(NULL, 0);
				return datum_api_OK(connection);
			}
			break;
		}
	
		case 'c': {
			if (!strcmp(url, "/clients")) {
				return datum_api_client_dashboard(connection);
			}
			if ((int_method==2) && (!strcmp(url, "/cmd"))) {
				if (con_info) {
					return datum_api_cmd(connection, con_info->data, con_info->data_size);
				} else {
					return MHD_NO;
				}
			}
			break;
		}
		case 't': {
			if (!strcmp(url, "/threads")) {
				return datum_api_thread_dashboard(connection);
			}
			if (!strcmp(url, "/testnet_fastforward")) {
				// Get the time parameter from the URL query
				time_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "ts");

				uint32_t t = -1000;
				if (time_str != NULL) {
					// Convert the time parameter to uint32_t
					t = (int)strtoul(time_str, NULL, 10);
				}
			
				datum_blocktemplates_notifynew("T", t);
				return datum_api_OK(connection);
			}
			break;
		}
		
		default: break;
	
	}
	
	const char *error_response = "<H1>Not found</H1>";
	response = MHD_create_response_from_buffer(strlen(error_response), (void *)error_response, MHD_RESPMEM_PERSISTENT);
	MHD_add_response_header(response, "Content-Type", "text/html");
	ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
	MHD_destroy_response (response);
	return ret;
	
}

void *datum_api_thread(void *ptr) {

	struct MHD_Daemon *daemon;

	if (!datum_config.api_listen_port) {
		DLOG_INFO("No API port configured. API disabled.");
		return NULL;
	}

	daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD, datum_config.api_listen_port, NULL, NULL, &datum_api_answer, NULL,
		MHD_OPTION_CONNECTION_LIMIT, 128,
		MHD_OPTION_NOTIFY_COMPLETED, datum_api_request_completed, NULL,
		MHD_OPTION_END);
		
	if (!daemon) {
		DLOG_FATAL("Unable to start daemon for API");
		panic_from_thread(__LINE__);
		return NULL;
	}

	DLOG_INFO("API listening on port %d", datum_config.api_listen_port);

	while(1) {
		sleep(3);
	}
	

}


int datum_api_init(void) {

	pthread_t pthread_datum_api_thread;

	if (!datum_config.api_listen_port) {
		DLOG_INFO("INFO: No API port configured. API disabled.");
		return 0;
	}
	pthread_create(&pthread_datum_api_thread, NULL, datum_api_thread, NULL);
	
	return 0;

}
