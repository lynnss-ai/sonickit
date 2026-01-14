/**
 * @file sip_core.c
 * @brief SIP Protocol Core Implementation
 */

#include "sip/sip_core.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ============================================
 * Helper Functions
 * ============================================ */

static char *skip_whitespace(char *str)
{
    while (*str && isspace((unsigned char)*str)) str++;
    return str;
}

static char *find_crlf(char *str)
{
    while (*str) {
        if (str[0] == '\r' && str[1] == '\n') return str;
        str++;
    }
    return NULL;
}

static void trim_trailing(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

/* Random number generator for IDs */
static uint32_t sip_random(void)
{
    static uint32_t seed = 0;
    if (seed == 0) {
#ifdef _WIN32
        seed = GetTickCount() ^ (uint32_t)time(NULL);
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        seed = (uint32_t)(tv.tv_sec ^ tv.tv_usec);
#endif
    }
    seed = seed * 1103515245 + 12345;
    return seed;
}

/* ============================================
 * Method String Conversion
 * ============================================ */

static const char *method_strings[] = {
    "UNKNOWN",
    "INVITE",
    "ACK",
    "BYE",
    "CANCEL",
    "REGISTER",
    "OPTIONS",
    "PRACK",
    "SUBSCRIBE",
    "NOTIFY",
    "PUBLISH",
    "INFO",
    "REFER",
    "MESSAGE",
    "UPDATE"
};

const char *sip_method_to_string(sip_method_t method)
{
    if (method >= 0 && method <= SIP_METHOD_UPDATE) {
        return method_strings[method];
    }
    return "UNKNOWN";
}

sip_method_t sip_method_from_string(const char *str)
{
    if (!str) return SIP_METHOD_UNKNOWN;
    
    for (int i = 1; i <= SIP_METHOD_UPDATE; i++) {
        if (strcasecmp(str, method_strings[i]) == 0) {
            return (sip_method_t)i;
        }
    }
    return SIP_METHOD_UNKNOWN;
}

const char *sip_status_reason(int status_code)
{
    switch (status_code) {
        case 100: return "Trying";
        case 180: return "Ringing";
        case 181: return "Call Is Being Forwarded";
        case 182: return "Queued";
        case 183: return "Session Progress";
        case 200: return "OK";
        case 202: return "Accepted";
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Moved Temporarily";
        case 305: return "Use Proxy";
        case 380: return "Alternative Service";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Timeout";
        case 480: return "Temporarily Unavailable";
        case 481: return "Call/Transaction Does Not Exist";
        case 486: return "Busy Here";
        case 487: return "Request Terminated";
        case 488: return "Not Acceptable Here";
        case 500: return "Server Internal Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Server Time-out";
        case 600: return "Busy Everywhere";
        case 603: return "Decline";
        default: return "Unknown";
    }
}

/* ============================================
 * SIP URI Functions
 * ============================================ */

voice_error_t sip_uri_parse(const char *str, sip_uri_t *uri)
{
    if (!str || !uri) return VOICE_ERROR_INVALID_PARAM;
    
    memset(uri, 0, sizeof(sip_uri_t));
    uri->port = SIP_DEFAULT_PORT;
    
    const char *p = str;
    
    /* Parse scheme */
    if (strncasecmp(p, "sips:", 5) == 0) {
        uri->scheme = SIP_URI_SCHEME_SIPS;
        uri->port = SIP_DEFAULT_TLS_PORT;
        p += 5;
    } else if (strncasecmp(p, "sip:", 4) == 0) {
        uri->scheme = SIP_URI_SCHEME_SIP;
        p += 4;
    } else if (strncasecmp(p, "tel:", 4) == 0) {
        uri->scheme = SIP_URI_SCHEME_TEL;
        p += 4;
    } else {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    /* Parse user@host */
    const char *at = strchr(p, '@');
    const char *host_start;
    
    if (at) {
        /* Has user part */
        size_t user_len = at - p;
        if (user_len >= sizeof(uri->user)) user_len = sizeof(uri->user) - 1;
        strncpy(uri->user, p, user_len);
        host_start = at + 1;
    } else {
        host_start = p;
    }
    
    /* Parse host:port */
    const char *colon = strchr(host_start, ':');
    const char *semi = strchr(host_start, ';');
    const char *host_end = semi ? semi : (host_start + strlen(host_start));
    
    if (colon && (colon < host_end)) {
        /* Has port */
        size_t host_len = colon - host_start;
        if (host_len >= sizeof(uri->host)) host_len = sizeof(uri->host) - 1;
        strncpy(uri->host, host_start, host_len);
        uri->port = (uint16_t)atoi(colon + 1);
    } else {
        size_t host_len = host_end - host_start;
        if (host_len >= sizeof(uri->host)) host_len = sizeof(uri->host) - 1;
        strncpy(uri->host, host_start, host_len);
    }
    
    /* Parse parameters */
    if (semi) {
        strncpy(uri->parameters, semi + 1, sizeof(uri->parameters) - 1);
        
        /* Extract transport if present */
        const char *transport = strstr(uri->parameters, "transport=");
        if (transport) {
            transport += 10;
            const char *end = strchr(transport, ';');
            size_t len = end ? (size_t)(end - transport) : strlen(transport);
            if (len >= sizeof(uri->transport)) len = sizeof(uri->transport) - 1;
            strncpy(uri->transport, transport, len);
        }
    }
    
    return VOICE_OK;
}

voice_error_t sip_uri_to_string(const sip_uri_t *uri, char *buffer, size_t size)
{
    if (!uri || !buffer || size == 0) return VOICE_ERROR_INVALID_PARAM;
    
    const char *scheme = (uri->scheme == SIP_URI_SCHEME_SIPS) ? "sips" : 
                         (uri->scheme == SIP_URI_SCHEME_TEL) ? "tel" : "sip";
    
    int len;
    if (uri->user[0]) {
        if (uri->port && uri->port != SIP_DEFAULT_PORT) {
            len = snprintf(buffer, size, "%s:%s@%s:%u", 
                          scheme, uri->user, uri->host, uri->port);
        } else {
            len = snprintf(buffer, size, "%s:%s@%s", 
                          scheme, uri->user, uri->host);
        }
    } else {
        if (uri->port && uri->port != SIP_DEFAULT_PORT) {
            len = snprintf(buffer, size, "%s:%s:%u", 
                          scheme, uri->host, uri->port);
        } else {
            len = snprintf(buffer, size, "%s:%s", scheme, uri->host);
        }
    }
    
    if (len >= (int)size) {
        return VOICE_ERROR_OVERFLOW;
    }
    
    return VOICE_OK;
}

bool sip_uri_equals(const sip_uri_t *a, const sip_uri_t *b)
{
    if (!a || !b) return false;
    if (a->scheme != b->scheme) return false;
    if (strcasecmp(a->host, b->host) != 0) return false;
    if (a->port != b->port) return false;
    if (strcmp(a->user, b->user) != 0) return false;
    return true;
}

/* ============================================
 * ID Generation Functions
 * ============================================ */

void sip_generate_call_id(char *buffer, size_t size, const char *host)
{
    if (!buffer || size == 0) return;
    
    uint32_t r1 = sip_random();
    uint32_t r2 = sip_random();
    
    snprintf(buffer, size, "%08x%08x@%s", r1, r2, host ? host : "localhost");
}

void sip_generate_branch(char *buffer, size_t size)
{
    if (!buffer || size == 0) return;
    
    uint32_t r1 = sip_random();
    uint32_t r2 = sip_random();
    
    /* RFC 3261 magic cookie */
    snprintf(buffer, size, "z9hG4bK%08x%08x", r1, r2);
}

void sip_generate_tag(char *buffer, size_t size)
{
    if (!buffer || size == 0) return;
    
    uint32_t r = sip_random();
    snprintf(buffer, size, "%08x", r);
}

/* ============================================
 * SIP Message Functions
 * ============================================ */

sip_message_t *sip_message_create(void)
{
    sip_message_t *msg = (sip_message_t *)calloc(1, sizeof(sip_message_t));
    if (msg) {
        msg->max_forwards = 70;
    }
    return msg;
}

void sip_message_destroy(sip_message_t *msg)
{
    if (!msg) return;
    if (msg->body) free(msg->body);
    if (msg->raw) free(msg->raw);
    free(msg);
}

/* Parse address header (From/To/Contact) */
static voice_error_t parse_address(const char *value, sip_address_t *addr)
{
    if (!value || !addr) return VOICE_ERROR_INVALID_PARAM;
    
    memset(addr, 0, sizeof(sip_address_t));
    
    const char *p = value;
    
    /* Check for display name */
    const char *lt = strchr(p, '<');
    if (lt) {
        /* Has display name */
        if (p[0] == '"') {
            const char *quote_end = strchr(p + 1, '"');
            if (quote_end) {
                size_t len = quote_end - p - 1;
                if (len >= sizeof(addr->display_name)) len = sizeof(addr->display_name) - 1;
                strncpy(addr->display_name, p + 1, len);
            }
        } else {
            size_t len = lt - p;
            while (len > 0 && isspace((unsigned char)p[len - 1])) len--;
            if (len >= sizeof(addr->display_name)) len = sizeof(addr->display_name) - 1;
            strncpy(addr->display_name, p, len);
        }
        
        /* Parse URI between < > */
        const char *gt = strchr(lt + 1, '>');
        if (gt) {
            char uri_buf[SIP_MAX_URI_LENGTH];
            size_t len = gt - lt - 1;
            if (len >= sizeof(uri_buf)) len = sizeof(uri_buf) - 1;
            strncpy(uri_buf, lt + 1, len);
            uri_buf[len] = '\0';
            sip_uri_parse(uri_buf, &addr->uri);
        }
        
        /* Look for tag parameter after > */
        const char *tag = strstr(value, ";tag=");
        if (tag) {
            tag += 5;
            const char *end = strchr(tag, ';');
            size_t len = end ? (size_t)(end - tag) : strlen(tag);
            if (len >= sizeof(addr->tag)) len = sizeof(addr->tag) - 1;
            strncpy(addr->tag, tag, len);
        }
    } else {
        /* No angle brackets, just URI */
        sip_uri_parse(value, &addr->uri);
    }
    
    return VOICE_OK;
}

voice_error_t sip_message_parse(const char *buffer, size_t size, sip_message_t *msg)
{
    if (!buffer || !msg || size == 0) return VOICE_ERROR_INVALID_PARAM;
    
    memset(msg, 0, sizeof(sip_message_t));
    msg->max_forwards = 70;
    
    /* Make a mutable copy */
    char *buf = (char *)malloc(size + 1);
    if (!buf) return VOICE_ERROR_OUT_OF_MEMORY;
    memcpy(buf, buffer, size);
    buf[size] = '\0';
    
    char *line = buf;
    char *next;
    bool first_line = true;
    
    while (line && *line) {
        next = find_crlf(line);
        if (next) {
            *next = '\0';
            next += 2;  /* Skip CRLF */
        }
        
        if (first_line) {
            first_line = false;
            
            /* Parse first line - either request or response */
            if (strncmp(line, "SIP/2.0 ", 8) == 0) {
                /* Response */
                msg->type = SIP_MSG_RESPONSE;
                line += 8;
                msg->status_code = atoi(line);
                char *sp = strchr(line, ' ');
                if (sp) {
                    strncpy(msg->reason_phrase, sp + 1, sizeof(msg->reason_phrase) - 1);
                    trim_trailing(msg->reason_phrase);
                }
            } else {
                /* Request */
                msg->type = SIP_MSG_REQUEST;
                char *sp = strchr(line, ' ');
                if (sp) {
                    *sp = '\0';
                    msg->method = sip_method_from_string(line);
                    
                    char *uri_end = strchr(sp + 1, ' ');
                    if (uri_end) {
                        *uri_end = '\0';
                        sip_uri_parse(sp + 1, &msg->request_uri);
                    }
                }
            }
        } else if (*line == '\0') {
            /* Empty line - end of headers, start of body */
            if (next && msg->content_length > 0) {
                msg->body = (char *)malloc(msg->content_length + 1);
                if (msg->body) {
                    memcpy(msg->body, next, msg->content_length);
                    msg->body[msg->content_length] = '\0';
                    msg->body_size = msg->content_length;
                }
            }
            break;
        } else {
            /* Parse header */
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char *name = line;
                char *value = skip_whitespace(colon + 1);
                trim_trailing(value);
                
                /* Handle known headers */
                if (strcasecmp(name, "Via") == 0 || strcasecmp(name, "v") == 0) {
                    if (msg->via_count < 8) {
                        sip_via_t *via = &msg->via[msg->via_count++];
                        /* Parse Via header */
                        if (strncasecmp(value, "SIP/2.0/", 8) == 0) {
                            char *proto_end = strchr(value + 8, ' ');
                            if (proto_end) {
                                size_t tlen = proto_end - value - 8;
                                if (tlen >= sizeof(via->transport)) tlen = sizeof(via->transport) - 1;
                                strncpy(via->transport, value + 8, tlen);
                                
                                /* Parse host:port */
                                char *host_start = skip_whitespace(proto_end + 1);
                                char *semi = strchr(host_start, ';');
                                char *col = strchr(host_start, ':');
                                
                                if (col && (!semi || col < semi)) {
                                    size_t hlen = col - host_start;
                                    if (hlen >= sizeof(via->host)) hlen = sizeof(via->host) - 1;
                                    strncpy(via->host, host_start, hlen);
                                    via->port = (uint16_t)atoi(col + 1);
                                } else {
                                    size_t hlen = semi ? (size_t)(semi - host_start) : strlen(host_start);
                                    if (hlen >= sizeof(via->host)) hlen = sizeof(via->host) - 1;
                                    strncpy(via->host, host_start, hlen);
                                    via->port = SIP_DEFAULT_PORT;
                                }
                                
                                /* Parse branch */
                                const char *branch = strstr(value, "branch=");
                                if (branch) {
                                    branch += 7;
                                    const char *end = strchr(branch, ';');
                                    size_t blen = end ? (size_t)(end - branch) : strlen(branch);
                                    if (blen >= sizeof(via->branch)) blen = sizeof(via->branch) - 1;
                                    strncpy(via->branch, branch, blen);
                                }
                            }
                        }
                    }
                } else if (strcasecmp(name, "From") == 0 || strcasecmp(name, "f") == 0) {
                    parse_address(value, &msg->from);
                } else if (strcasecmp(name, "To") == 0 || strcasecmp(name, "t") == 0) {
                    parse_address(value, &msg->to);
                } else if (strcasecmp(name, "Contact") == 0 || strcasecmp(name, "m") == 0) {
                    parse_address(value, &msg->contact);
                } else if (strcasecmp(name, "Call-ID") == 0 || strcasecmp(name, "i") == 0) {
                    strncpy(msg->call_id.call_id, value, sizeof(msg->call_id.call_id) - 1);
                } else if (strcasecmp(name, "CSeq") == 0) {
                    msg->cseq.seq = (uint32_t)atoi(value);
                    char *method = strchr(value, ' ');
                    if (method) {
                        msg->cseq.method = sip_method_from_string(skip_whitespace(method));
                    }
                } else if (strcasecmp(name, "Max-Forwards") == 0) {
                    msg->max_forwards = atoi(value);
                } else if (strcasecmp(name, "Content-Length") == 0 || strcasecmp(name, "l") == 0) {
                    msg->content_length = atoi(value);
                } else if (strcasecmp(name, "Content-Type") == 0 || strcasecmp(name, "c") == 0) {
                    strncpy(msg->content_type, value, sizeof(msg->content_type) - 1);
                } else if (strcasecmp(name, "Expires") == 0) {
                    msg->expires = atoi(value);
                } else {
                    /* Store as generic header */
                    if (msg->header_count < SIP_MAX_HEADERS) {
                        sip_header_t *hdr = &msg->headers[msg->header_count++];
                        strncpy(hdr->name, name, sizeof(hdr->name) - 1);
                        strncpy(hdr->value, value, sizeof(hdr->value) - 1);
                    }
                }
            }
        }
        
        line = next;
    }
    
    free(buf);
    return VOICE_OK;
}

voice_error_t sip_message_serialize(const sip_message_t *msg, char *buffer, 
                                     size_t buffer_size, size_t *out_size)
{
    if (!msg || !buffer || buffer_size == 0) return VOICE_ERROR_INVALID_PARAM;
    
    char *p = buffer;
    size_t remaining = buffer_size;
    int len;
    
    /* First line */
    if (msg->type == SIP_MSG_REQUEST) {
        char uri_str[SIP_MAX_URI_LENGTH];
        sip_uri_to_string(&msg->request_uri, uri_str, sizeof(uri_str));
        len = snprintf(p, remaining, "%s %s SIP/2.0\r\n",
                      sip_method_to_string(msg->method), uri_str);
    } else {
        len = snprintf(p, remaining, "SIP/2.0 %d %s\r\n",
                      msg->status_code, msg->reason_phrase);
    }
    
    if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
    p += len;
    remaining -= len;
    
    /* Via headers */
    for (int i = 0; i < msg->via_count; i++) {
        const sip_via_t *via = &msg->via[i];
        len = snprintf(p, remaining, "Via: SIP/2.0/%s %s:%u;branch=%s\r\n",
                      via->transport[0] ? via->transport : "UDP",
                      via->host, via->port, via->branch);
        if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
        p += len;
        remaining -= len;
    }
    
    /* From */
    {
        char uri_str[SIP_MAX_URI_LENGTH];
        sip_uri_to_string(&msg->from.uri, uri_str, sizeof(uri_str));
        if (msg->from.display_name[0]) {
            len = snprintf(p, remaining, "From: \"%s\" <%s>;tag=%s\r\n",
                          msg->from.display_name, uri_str, msg->from.tag);
        } else {
            len = snprintf(p, remaining, "From: <%s>;tag=%s\r\n",
                          uri_str, msg->from.tag);
        }
        if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
        p += len;
        remaining -= len;
    }
    
    /* To */
    {
        char uri_str[SIP_MAX_URI_LENGTH];
        sip_uri_to_string(&msg->to.uri, uri_str, sizeof(uri_str));
        if (msg->to.tag[0]) {
            len = snprintf(p, remaining, "To: <%s>;tag=%s\r\n", uri_str, msg->to.tag);
        } else {
            len = snprintf(p, remaining, "To: <%s>\r\n", uri_str);
        }
        if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
        p += len;
        remaining -= len;
    }
    
    /* Call-ID */
    len = snprintf(p, remaining, "Call-ID: %s\r\n", msg->call_id.call_id);
    if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
    p += len;
    remaining -= len;
    
    /* CSeq */
    len = snprintf(p, remaining, "CSeq: %u %s\r\n",
                  msg->cseq.seq, sip_method_to_string(msg->cseq.method));
    if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
    p += len;
    remaining -= len;
    
    /* Contact (if present) */
    if (msg->contact.uri.host[0]) {
        char uri_str[SIP_MAX_URI_LENGTH];
        sip_uri_to_string(&msg->contact.uri, uri_str, sizeof(uri_str));
        len = snprintf(p, remaining, "Contact: <%s>\r\n", uri_str);
        if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
        p += len;
        remaining -= len;
    }
    
    /* Max-Forwards */
    len = snprintf(p, remaining, "Max-Forwards: %d\r\n", msg->max_forwards);
    if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
    p += len;
    remaining -= len;
    
    /* Additional headers */
    for (int i = 0; i < msg->header_count; i++) {
        len = snprintf(p, remaining, "%s: %s\r\n",
                      msg->headers[i].name, msg->headers[i].value);
        if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
        p += len;
        remaining -= len;
    }
    
    /* Content-Type and Content-Length */
    if (msg->body && msg->body_size > 0) {
        len = snprintf(p, remaining, "Content-Type: %s\r\n",
                      msg->content_type[0] ? msg->content_type : "application/sdp");
        if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
        p += len;
        remaining -= len;
    }
    
    len = snprintf(p, remaining, "Content-Length: %zu\r\n\r\n",
                  msg->body_size);
    if (len >= (int)remaining) return VOICE_ERROR_OVERFLOW;
    p += len;
    remaining -= len;
    
    /* Body */
    if (msg->body && msg->body_size > 0) {
        if (msg->body_size > remaining) return VOICE_ERROR_OVERFLOW;
        memcpy(p, msg->body, msg->body_size);
        p += msg->body_size;
    }
    
    if (out_size) {
        *out_size = buffer_size - remaining + (msg->body_size > 0 ? msg->body_size : 0);
    }
    
    return VOICE_OK;
}

voice_error_t sip_message_add_header(sip_message_t *msg,
                                      const char *name,
                                      const char *value)
{
    if (!msg || !name || !value) return VOICE_ERROR_INVALID_PARAM;
    if (msg->header_count >= SIP_MAX_HEADERS) return VOICE_ERROR_OVERFLOW;
    
    sip_header_t *hdr = &msg->headers[msg->header_count++];
    strncpy(hdr->name, name, sizeof(hdr->name) - 1);
    strncpy(hdr->value, value, sizeof(hdr->value) - 1);
    
    return VOICE_OK;
}

const char *sip_message_get_header(const sip_message_t *msg, const char *name)
{
    if (!msg || !name) return NULL;
    
    for (int i = 0; i < msg->header_count; i++) {
        if (strcasecmp(msg->headers[i].name, name) == 0) {
            return msg->headers[i].value;
        }
    }
    return NULL;
}

voice_error_t sip_message_set_body(sip_message_t *msg,
                                    const char *content_type,
                                    const char *body,
                                    size_t body_size)
{
    if (!msg) return VOICE_ERROR_INVALID_PARAM;
    
    if (msg->body) {
        free(msg->body);
        msg->body = NULL;
    }
    
    if (body && body_size > 0) {
        msg->body = (char *)malloc(body_size + 1);
        if (!msg->body) return VOICE_ERROR_OUT_OF_MEMORY;
        memcpy(msg->body, body, body_size);
        msg->body[body_size] = '\0';
        msg->body_size = body_size;
        msg->content_length = (int)body_size;
        
        if (content_type) {
            strncpy(msg->content_type, content_type, sizeof(msg->content_type) - 1);
        }
    } else {
        msg->body_size = 0;
        msg->content_length = 0;
    }
    
    return VOICE_OK;
}

voice_error_t sip_message_create_response(sip_message_t *response,
                                           const sip_message_t *request,
                                           int status_code,
                                           const char *reason)
{
    if (!response || !request) return VOICE_ERROR_INVALID_PARAM;
    if (request->type != SIP_MSG_REQUEST) return VOICE_ERROR_INVALID_PARAM;
    
    memset(response, 0, sizeof(sip_message_t));
    
    response->type = SIP_MSG_RESPONSE;
    response->status_code = status_code;
    strncpy(response->reason_phrase, 
            reason ? reason : sip_status_reason(status_code),
            sizeof(response->reason_phrase) - 1);
    
    /* Copy Via headers */
    response->via_count = request->via_count;
    for (int i = 0; i < request->via_count; i++) {
        response->via[i] = request->via[i];
    }
    
    /* Copy From, To, Call-ID, CSeq */
    response->from = request->from;
    response->to = request->to;
    response->call_id = request->call_id;
    response->cseq = request->cseq;
    
    /* Add To tag for responses */
    if (!response->to.tag[0]) {
        sip_generate_tag(response->to.tag, sizeof(response->to.tag));
    }
    
    response->max_forwards = 70;
    
    return VOICE_OK;
}

voice_error_t sip_message_create_invite(sip_message_t *msg,
                                         const sip_uri_t *to,
                                         const sip_uri_t *from,
                                         const char *call_id,
                                         uint32_t cseq,
                                         const char *sdp)
{
    if (!msg || !to || !from) return VOICE_ERROR_INVALID_PARAM;
    
    memset(msg, 0, sizeof(sip_message_t));
    
    msg->type = SIP_MSG_REQUEST;
    msg->method = SIP_METHOD_INVITE;
    msg->request_uri = *to;
    
    msg->from.uri = *from;
    sip_generate_tag(msg->from.tag, sizeof(msg->from.tag));
    
    msg->to.uri = *to;
    
    if (call_id) {
        strncpy(msg->call_id.call_id, call_id, sizeof(msg->call_id.call_id) - 1);
    } else {
        sip_generate_call_id(msg->call_id.call_id, sizeof(msg->call_id.call_id), from->host);
    }
    
    msg->cseq.seq = cseq;
    msg->cseq.method = SIP_METHOD_INVITE;
    msg->max_forwards = 70;
    
    /* Contact */
    msg->contact.uri = *from;
    
    if (sdp) {
        sip_message_set_body(msg, "application/sdp", sdp, strlen(sdp));
    }
    
    return VOICE_OK;
}

voice_error_t sip_message_create_ack(sip_message_t *msg,
                                      const sip_message_t *invite)
{
    if (!msg || !invite) return VOICE_ERROR_INVALID_PARAM;
    
    memset(msg, 0, sizeof(sip_message_t));
    
    msg->type = SIP_MSG_REQUEST;
    msg->method = SIP_METHOD_ACK;
    msg->request_uri = invite->request_uri;
    
    msg->from = invite->from;
    msg->to = invite->to;
    msg->call_id = invite->call_id;
    
    msg->cseq.seq = invite->cseq.seq;
    msg->cseq.method = SIP_METHOD_ACK;
    msg->max_forwards = 70;
    
    return VOICE_OK;
}

voice_error_t sip_message_create_bye(sip_message_t *msg,
                                      const char *call_id,
                                      const sip_uri_t *to,
                                      const sip_uri_t *from,
                                      uint32_t cseq)
{
    if (!msg || !call_id || !to || !from) return VOICE_ERROR_INVALID_PARAM;
    
    memset(msg, 0, sizeof(sip_message_t));
    
    msg->type = SIP_MSG_REQUEST;
    msg->method = SIP_METHOD_BYE;
    msg->request_uri = *to;
    
    msg->from.uri = *from;
    msg->to.uri = *to;
    strncpy(msg->call_id.call_id, call_id, sizeof(msg->call_id.call_id) - 1);
    
    msg->cseq.seq = cseq;
    msg->cseq.method = SIP_METHOD_BYE;
    msg->max_forwards = 70;
    
    return VOICE_OK;
}

voice_error_t sip_message_create_register(sip_message_t *msg,
                                           const sip_uri_t *registrar,
                                           const sip_uri_t *aor,
                                           const sip_address_t *contact,
                                           int expires)
{
    if (!msg || !registrar || !aor) return VOICE_ERROR_INVALID_PARAM;
    
    memset(msg, 0, sizeof(sip_message_t));
    
    msg->type = SIP_MSG_REQUEST;
    msg->method = SIP_METHOD_REGISTER;
    msg->request_uri = *registrar;
    msg->request_uri.user[0] = '\0';  /* REGISTER URI has no user part */
    
    msg->from.uri = *aor;
    sip_generate_tag(msg->from.tag, sizeof(msg->from.tag));
    
    msg->to.uri = *aor;
    
    sip_generate_call_id(msg->call_id.call_id, sizeof(msg->call_id.call_id), 
                         registrar->host);
    
    msg->cseq.seq = 1;
    msg->cseq.method = SIP_METHOD_REGISTER;
    msg->max_forwards = 70;
    msg->expires = expires;
    
    if (contact) {
        msg->contact = *contact;
    } else {
        msg->contact.uri = *aor;
    }
    
    /* Add Expires header */
    char expires_str[16];
    snprintf(expires_str, sizeof(expires_str), "%d", expires);
    sip_message_add_header(msg, "Expires", expires_str);
    
    return VOICE_OK;
}
