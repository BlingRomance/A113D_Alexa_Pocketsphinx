/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2010 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/*
 * continuous.c - Simple pocketsphinx command-line application to test
 *                both continuous listening/silence filtering from microphone
 *                and continuous file transcription.
 */

/*
 * This is a simple example of pocketsphinx application that uses continuous listening
 * with silence filtering to automatically segment a continuous stream of audio input
 * into utterances that are then decoded.
 * 
 * Remarks:
 *   - Each utterance is ended when a silence segment of at least 1 sec is recognized.
 *   - Single-threaded implementation for portability.
 *   - Uses audio library; can be replaced with an equivalent custom library.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#else
#include <sys/select.h>
#endif

#include <sphinxbase/err.h>
#include <sphinxbase/ad.h>

#include "pocketsphinx.h"

// 2018/05/04 Bling Added
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct snd_msgbuf {
    long mtype;
    char mtext[20];
};

struct rcv_msgbuf {
    long mtype;
    char mtext[20];
};

FILE *pFile;
char pBuffer[20];
// 2018/05/04 Bling Added

static const arg_t cont_args_def[] = {
    POCKETSPHINX_OPTIONS,
    /* Argument file. */
    {"-argfile",
     ARG_STRING,
     NULL,
     "Argument file giving extra arguments."},
    {"-adcdev",
     ARG_STRING,
     NULL,
     "Name of audio device to use for input."},
    {"-infile",
     ARG_STRING,
     NULL,
     "Audio file to transcribe."},
    {"-inmic",
     ARG_BOOLEAN,
     "no",
     "Transcribe audio from microphone."},
    {"-time",
     ARG_BOOLEAN,
     "no",
     "Print word times in file transcription."},
    CMDLN_EMPTY_OPTION
};

static ps_decoder_t *ps;
static cmd_ln_t *config;

/* Sleep for specified msec */
static void
sleep_msec(int32 ms)
{
#if (defined(_WIN32) && !defined(GNUWINCE)) || defined(_WIN32_WCE)
    Sleep(ms);
#else
    /* ------------------- Unix ------------------ */
    struct timeval tmo;

    tmo.tv_sec = 0;
    tmo.tv_usec = ms * 1000;

    select(0, NULL, NULL, NULL, &tmo);
#endif
}

/*
 * Main utterance processing loop:
 *     for (;;) {
 *        start utterance and wait for speech to process
 *        decoding till end-of-utterance silence will be detected
 *        print utterance result;
 *     }
 */
static void
recognize_from_microphone()
{
    ad_rec_t *ad;
    int16 adbuf[2048];
    uint8 utt_started, in_speech;
    int32 k;
    char const *hyp;

    // 2018/05/04 Bling Added
    struct snd_msgbuf snd_buf;
    struct rcv_msgbuf rcv_buf;
    int snd_sqid, rcv_sqid;
    key_t key;
    int m_strlen;

    // snd
    if ((key = ftok("/home/parallels/corpus.txt", 66)) == -1) {
        perror("ftok");
        exit(1);
    }

    if ((snd_sqid = msgget(key, 0644 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
    }

    snd_buf.mtype = 1;

    if ((pFile = fopen("/home/parallels/corpus.txt", "r")) == NULL) {
        perror("fopen");
        exit(1);
    }

    while (fgets(pBuffer, 20, pFile) != NULL) {
    }

    fclose(pFile);
    m_strlen = strlen(pBuffer) - 1;

    // 2018/05/04 Bling Added
    if (ps_start_utt(ps) < 0) {
        E_FATAL("Failed to start utterance\n");
    }
    utt_started = FALSE;
    E_INFO("Ready....\n");

    for (;;) {
        if (!strcmp(rcv_buf.mtext, "OK")) {
            if ((k = ad_read(ad, adbuf, 2048)) < 0) {
                E_FATAL("Failed to read audio\n");
            }
            ps_process_raw(ps, adbuf, k, FALSE, FALSE);
            in_speech = ps_get_in_speech(ps);
        } else {
            // rcv
            if ((key = ftok("/home/parallels/a113d.txt", 66)) == -1) {
                perror("ftok");
                exit(1);
            }

            if ((rcv_sqid = msgget(key, 0644 | IPC_CREAT)) == -1) {
                perror("msgget");
                exit(1);
            }
            
            if (msgrcv(rcv_sqid, &rcv_buf, sizeof(rcv_buf), 0, 0) == -1) {
                perror("msgrcv");
                exit(1);
            }

            if ((ad = ad_open_dev(cmd_ln_str_r(config, "-adcdev"), (int) cmd_ln_float32_r(config, "-samprate"))) == NULL) {
                E_FATAL("Failed to open audio device\n");
            }

            if (ad_start_rec(ad) < 0) {
                E_FATAL("Failed to start recording\n");
            }

            printf("%s\n", rcv_buf.mtext);
        }

        if (in_speech && !utt_started) {
            utt_started = TRUE;
            E_INFO("Listening...\n");
        }

        if (!in_speech && utt_started) {
            /* speech -> silence transition, time to start new utterance  */
            ps_end_utt(ps);
            hyp = ps_get_hyp(ps, NULL);

            if (hyp != NULL) {
                // 2018/05/04 Bling Added
                if (!strncmp(hyp, pBuffer, m_strlen) && strlen(hyp) == m_strlen) {
                    strncpy(snd_buf.mtext, hyp, m_strlen);

                    ad_close(ad);

                    if (msgsnd(snd_sqid, &snd_buf, sizeof(snd_buf), 0) == -1) {
                        perror("msgsnd");
                        exit(1);
                    }
                    
                    printf("%s\n", snd_buf.mtext);
                    strcpy(rcv_buf.mtext, "");
                }
                // 2018/05/04 Bling Added
                
                printf("%s\n", hyp);
                fflush(stdout);
            }

            if (ps_start_utt(ps) < 0)
                E_FATAL("Failed to start utterance\n");
            utt_started = FALSE;
            E_INFO("Ready....\n");
        }
        sleep_msec(100);
    }
    ad_close(ad);
}

int
main(int argc, char** argv)
{
    char const *cfg;

    config = cmd_ln_parse_r(NULL, cont_args_def, argc, argv, TRUE);

    /* Handle argument file as -argfile. */
    if (config && (cfg = cmd_ln_str_r(config, "-argfile")) != NULL) {
        config = cmd_ln_parse_file_r(config, cont_args_def, cfg, FALSE);
    }

    if (config == NULL || (cmd_ln_str_r(config, "-infile") == NULL && cmd_ln_boolean_r(config, "-inmic") == FALSE)) {
	E_INFO("Specify '-infile <file.wav>' to recognize from file or '-inmic yes' to recognize from microphone.\n");
        cmd_ln_free_r(config);
	return 1;
    }

    ps_default_search_args(config);
    ps = ps_init(config);
    if (ps == NULL) {
        cmd_ln_free_r(config);
        return 1;
    }

    E_INFO("%s COMPILED ON: %s, AT: %s\n\n", argv[0], __DATE__, __TIME__);

    if (cmd_ln_str_r(config, "-infile") != NULL) {
        //recognize_from_file();
    } else if (cmd_ln_boolean_r(config, "-inmic")) {
        recognize_from_microphone();
    }

    ps_free(ps);
    cmd_ln_free_r(config);

    return 0;
}

#if defined(_WIN32_WCE)
#pragma comment(linker,"/entry:mainWCRTStartup")
#include <windows.h>
//Windows Mobile has the Unicode main only
int
wmain(int32 argc, wchar_t * wargv[])
{
    char **argv;
    size_t wlen;
    size_t len;
    int i;

    argv = malloc(argc * sizeof(char *));
    for (i = 0; i < argc; i++) {
        wlen = lstrlenW(wargv[i]);
        len = wcstombs(NULL, wargv[i], wlen);
        argv[i] = malloc(len + 1);
        wcstombs(argv[i], wargv[i], wlen);
    }

    //assuming ASCII parameters
    return main(argc, argv);
}
#endif
