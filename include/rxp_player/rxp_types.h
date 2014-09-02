#ifndef RXP_TYPES_H
#define RXP_TYPES_H

#define RXP_NONE 0

/* scheduler tasks */
#define RXP_TASK_DECODE 1
#define RXP_TASK_PLAY 2                
#define RXP_TASK_OPEN_FILE 3
#define RXP_TASK_CLOSE_FILE 4
#define RXP_TASK_STOP 5

/* decoder states */
#define RXP_DEC_STATE_NONE 0x0000          /* default state */
#define RXP_DEC_STATE_DECODING 0x0002      /* we've opened a i/o stream and we're decoding */
#define RXP_DEC_STATE_READY 0x0004         /* all packets have been decoded, file is closed */

/* player states */
#define RXP_PSTATE_NONE (1 << 0)             /* the player isn't doing anything */
#define RXP_PSTATE_PLAYING (1 << 1)          /* we're playing */
#define RXP_PSTATE_STOPPED (1 << 2)          /* when the user asked the player to stop. */
#define RXP_PSTATE_PAUSED (1 << 3)           /* when the player is paused */
#define RXP_PSTATE_DECODE_READY (1 << 4)     /* ready with decoding the file */
#define RXP_PSTATE_SHUTTING_DOWN (1 << 5)    /* when rxp_player_clear() is called; only used to make sure tat someone doesn't call clear multiple times. */

/* decoder events */
#define RXP_DEC_EVENT_READY 0x0001         /* we're ready with decoding all packets; decoding may stop */
#define RXP_DEC_EVENT_AUDIO_INFO 0x0002    /* we have found an audio stream and the samplerate/channels member has been set */
#define RXP_PLAYER_EVENT_RESET 0x0003      /* gets fired when the player is ready with playing all the video packets and you should "stop" rendering and the audio stream */
#define RXP_PLAYER_EVENT_PLAY 0x0004       /* gets fired when the decoder/scheduler is ready with pre-buffering and opening the file and we're kicking off playback */
 
/* scheduler states */
#define RXP_SCHED_STATE_NONE 0x0000
#define RXP_SCHED_STATE_STARTED 0x0001
#define RXP_SCHED_STATE_DECODING 0x0002

/* decoder types */
#define RXP_THEORA 1
#define RXP_VORBIS 2

/* rxp_packet types */
#define RXP_YUV420P 1 

#endif
