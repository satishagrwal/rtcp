#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MEDIA 8
#define LINE_LEN 256

typedef struct {
    char ip[64];
    int port;
    int as_kbps;
    int rs_bps;
    int rr_bps;
    int valid;
    char mtype[16];
} media_t;

typedef struct {
    char sess_ip[64];
    media_t media[MAX_MEDIA];
    int media_cnt;
    int sess_as_kbps;
    int sess_rs_bps;
    int sess_rr_bps;
} sdp_t;

/* ---------- Utility ---------- */

void apply_rtcp_defaults(media_t *m) {
    int rtcp_total = m->as_kbps * 1000 * 5 / 100;

    if (m->rs_bps < 0)
        m->rs_bps = rtcp_total * 25 / 100;
    if (m->rr_bps < 0)
        m->rr_bps = rtcp_total * 75 / 100;

    if (m->rs_bps > 4000) m->rs_bps = 4000;
    if (m->rr_bps > 5000) m->rr_bps = 5000;
}

/* ---------- SDP Parsing ---------- */

void init_sdp(sdp_t *sdp) {
    memset(sdp, 0, sizeof(*sdp));
    for (int i = 0; i < MAX_MEDIA; i++) {
        sdp->media[i].rs_bps = -1;
        sdp->media[i].rr_bps = -1;
    }
}

void parse_sdp(const char *file, sdp_t *sdp) {
    FILE *fp = fopen(file, "r");
    if (!fp) {
        perror(file);
        exit(1);
    }

    char line[LINE_LEN];
    media_t *cur = NULL;

    while (fgets(line, sizeof(line), fp)) {

        if (strncmp(line, "c=", 2) == 0) {
            char ip[64];
            sscanf(line, "c=IN IP6 %63s", ip);

            if (cur)
                strcpy(cur->ip, ip);
            else
                strcpy(sdp->sess_ip, ip);
        }

        else if (strncmp(line, "m=", 2) == 0) {
            cur = &sdp->media[sdp->media_cnt++];
            memset(cur, 0, sizeof(media_t));
            cur->rs_bps = cur->rr_bps = -1;
            cur->valid = 1;

            char mtype[16];
            int port = 0;
            if (sscanf(line, "m=%15s %d", mtype, &port) >= 1) {
                cur->port = port;
                strncpy(cur->mtype, mtype, sizeof(cur->mtype)-1);
                cur->mtype[sizeof(cur->mtype)-1] = '\0';
                /* validate port range: allow 0 (rejected) up to 65535 */
                if (cur->port < 0 || cur->port > 65535) {
                    fprintf(stderr, "Warning: invalid port %d in %s - treating as rejected (port=0)\n", cur->port, file);
                    cur->port = 0;
                }
            }
            /* initialize media with session-level b= values if present */
            cur->as_kbps = sdp->sess_as_kbps;
            if (sdp->sess_rs_bps >= 0) cur->rs_bps = sdp->sess_rs_bps;
            if (sdp->sess_rr_bps >= 0) cur->rr_bps = sdp->sess_rr_bps;
            strcpy(cur->ip, sdp->sess_ip);
        }

        else if (strncmp(line, "b=AS:", 5) == 0) {
            int v = atoi(line + 5);
            if (cur)
                cur->as_kbps = v;
            else
                sdp->sess_as_kbps = v;
        }

        else if (strncmp(line, "b=RS:", 5) == 0) {
            int v = atoi(line + 5);
            if (cur)
                cur->rs_bps = v;
            else
                sdp->sess_rs_bps = v;
        }

        else if (strncmp(line, "b=RR:", 5) == 0) {
            int v = atoi(line + 5);
            if (cur)
                cur->rr_bps = v;
            else
                sdp->sess_rr_bps = v;
        }
    }

    fclose(fp);
}

/* ---------- Matching Offer / Answer ---------- */

int media_accepted(media_t *offer, media_t *answer) {
    return offer->port > 0 && answer->port > 0;
}

/* ---------- Printing ---------- */

void print_flow(media_t *o, media_t *a,
                const char *o_ip, const char *a_ip) {

    apply_rtcp_defaults(o);
    apply_rtcp_defaults(a);

    /* Offer → Answer */
    printf("permit %dkbps from %s %d to %s %d\n",
           o->as_kbps, o_ip, o->port, a_ip, a->port);

    printf("permit %dbps from %s %d to %s %d\n",
           o->rs_bps, o_ip, o->port + 1, a_ip, a->port + 1);

    /* Answer → Offer */
    printf("permit %dkbps from %s %d to %s %d\n",
           a->as_kbps, a_ip, a->port, o_ip, o->port);

    printf("permit %dbps from %s %d to %s %d\n",
           a->rr_bps, a_ip, a->port + 1, o_ip, o->port + 1);
}

/* ---------- Main ---------- */

int main(int argc, char **argv) {

    if (argc != 3) {
        printf("Usage: %s offer.sdp answer.sdp\n", argv[0]);
        return 1;
    }

    sdp_t offer, answer;
    init_sdp(&offer);
    init_sdp(&answer);

    parse_sdp(argv[1], &offer);
    parse_sdp(argv[2], &answer);

    /* Sanity check: number of media lines should match between offer and answer; warn if mismatch */
    if (offer.media_cnt != answer.media_cnt) {
        fprintf(stderr, "Warning: m-line count mismatch: offer=%d answer=%d\n",
                offer.media_cnt, answer.media_cnt);
    }

    double offer_total = 0;
    double answer_total = 0;

    for (int i = 0; i < offer.media_cnt && i < answer.media_cnt; i++) {

        if (!media_accepted(&offer.media[i], &answer.media[i]))
            continue;

        print_flow(&offer.media[i], &answer.media[i],
                   offer.sess_ip, answer.sess_ip);

        offer_total += offer.media[i].as_kbps +
                       offer.media[i].rs_bps / 1000.0;

        answer_total += answer.media[i].as_kbps +
                        answer.media[i].rr_bps / 1000.0;
    }

    printf("Total uplink from %s ---> %.1f kbps\n",
           offer.sess_ip, offer_total);

    printf("Total uplink from %s ---> %.1f kbps\n",
           answer.sess_ip, answer_total);

    return 0;
}
