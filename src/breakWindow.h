#ifndef BREAK_WINDOW_H
#define BREAK_WINDOW_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Show the fullscreen break overlay window.
 *
 * breakSeconds: countdown duration in seconds.
 *
 * Returns true  if the user skipped the break (button or Ctrl+Q),
 *         false if the timer expired naturally.
 *
 * This call blocks until the window closes.
 */
bool breakWindowShow(int breakSeconds);

#ifdef __cplusplus
}
#endif

#endif