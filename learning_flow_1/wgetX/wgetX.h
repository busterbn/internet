/**
 *  Jiazi Yi
 *
 * LIX, Ecole Polytechnique
 * jiazi.yi@polytechnique.edu
 *
 * Updated by Pierre Pfister
 *
 * Cisco Systems
 * ppfister@cisco.com
 *
 */

#ifndef WGETX_H_
#define WGETX_H_

#include <pthread.h>

/* Structure used to store the buffer and buffer length when receiving the reply from an http server. */
typedef struct http_reply {

    /* The address of the buffer containing everything received from the server.
     * The memory is allocated using malloc, and its size can be increased with realloc. */
    char *reply_buffer;

    /* The total number of bytes in the reply */
    int reply_buffer_length;
} http_reply;

/**
 * \brief write the data to a file
 * \param path the path and name of the file
 * \param data the pointer of the buffer that to be written.
 * \param len the number of bytes to be written from the data buffer onto the file.
 */
void write_data(const char *path, const char * data, int len);

/**
 * \brief download a page using the http protocol
 * \param info the url information
 * \param reply the output of the request
 * \return 0 on success, an error code on failure
 */
int download_page(url_info *info, http_reply *reply);

/**
 * \brief return a string with a get http request
 * \param info the url information
 * \return the pointer to the get http request string. The pointer must be freed using 'free'.
 */
char* http_get_request(url_info *info);

/**
 * \brief process the http reply from server
 * \param reply the reply structure
 * \return a pointer to the first data byte
 */
char *read_http_reply(struct http_reply *reply);

/**
 * \brief check if the http reply is a redirect
 * \param reply the reply structure
 * \return redirect URL if it's a redirect, NULL otherwise
 */
char *check_redirect(struct http_reply *reply);

/* Structure to track visited URLs (thread-safe) */
typedef struct url_set {
    char **urls;
    int count;
    int capacity;
    pthread_mutex_t mutex;
} url_set;

/* Structure to pass data to download threads */
typedef struct download_task {
    char *url;
    char *base_url;
    char *output_dir;
    int depth;
    int max_depth;
    url_set *visited;
} download_task;

/**
 * \brief initialize a URL set
 * \param set the url set to initialize
 */
void url_set_init(url_set *set);

/**
 * \brief check if URL has been visited, add it if not
 * \param set the url set
 * \param url the url to check/add
 * \return 1 if already visited, 0 if newly added
 */
int url_set_check_and_add(url_set *set, const char *url);

/**
 * \brief free a URL set
 * \param set the url set to free
 */
void url_set_free(url_set *set);

/**
 * \brief extract all URLs from HTML content
 * \param html the HTML content
 * \param len the length of the HTML
 * \param urls output array of URLs (must be freed by caller)
 * \return number of URLs found
 */
int extract_urls_from_html(const char *html, int len, char ***urls);

/**
 * \brief normalize a URL (convert relative to absolute)
 * \param base_url the base URL
 * \param relative_url the relative URL
 * \return the absolute URL (must be freed by caller)
 */
char *normalize_url(const char *base_url, const char *relative_url);

/**
 * \brief generate local filename from URL
 * \param url the URL
 * \param base_url the base URL
 * \param output_dir the output directory
 * \return the local filename (must be freed by caller)
 */
char *url_to_filename(const char *url, const char *base_url, const char *output_dir);

/**
 * \brief rewrite URLs in HTML to point to local files
 * \param html the HTML content
 * \param len the length of the HTML
 * \param base_url the base URL
 * \param output_dir the output directory
 * \return the rewritten HTML (must be freed by caller)
 */
char *rewrite_html_urls(const char *html, int len, const char *base_url, const char *output_dir);

/**
 * \brief thread function to download and process a URL
 * \param arg the download_task structure
 */
void *download_thread(void *arg);

/**
 * \brief check if URL belongs to the same domain as base URL
 * \param url the URL to check
 * \param base_url the base URL
 * \return 1 if same domain, 0 otherwise
 */
int is_same_domain(const char *url, const char *base_url);

#endif /* WGETX_H_ */
