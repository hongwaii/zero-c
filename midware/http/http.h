/**
 * @file http.h
 * @brief HTTP client/server middleware — public API
 */
#ifndef MIDWARE_HTTP_H
#define MIDWARE_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Placeholder API ---- */
int http_init(void);
int http_get(const char *url, char *response, int response_len);
int http_post(const char *url, const char *body, char *response, int response_len);
void http_cleanup(void);

#ifdef __cplusplus
}
#endif
#endif /* MIDWARE_HTTP_H */
