/** \file   kvice_framecheck.h
 * \brief   Linux test build: a hash of every emulated frame
 */

#ifndef KVICE_FRAMECHECK_H
#define KVICE_FRAMECHECK_H

struct draw_buffer_s;

void kvice_frame_checksum(const struct draw_buffer_s *db);

/* test: KVICE_SCREENSHOT=<file> saves an IFF screenshot at frame
   KVICE_SCREENSHOT_FRAME (default 150), as the Amiga Display menu does */
struct video_canvas_s;
void kvice_frame_canvas(struct video_canvas_s *canvas);

/* test: second autostart from the presync (KVICE_AUTOSTART2_PRESYNC) */
void kvice_test_presync(void);

#endif
