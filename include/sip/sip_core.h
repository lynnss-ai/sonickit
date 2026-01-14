/**
 * @file sip_core.h
 * @brief SIP Protocol Core Definitions
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * Core SIP (Session Initiation Protocol) support for voice applications.
 * Based on RFC 3261 with essential extensions for voice calls.
 * 
 * Provides:
 * - SIP message parsing and generation
 * - Transaction layer
 * - Dialog management
 * - User Agent (UA) functionality
 */

#ifndef VOICE_SIP_CORE_H
#define VOICE_SIP_CORE_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Constants
 * ============================================ */

#define SIP_VERSION             "SIP/2.0"
#define SIP_DEFAULT_PORT        5060
#define SIP_DEFAULT_TLS_PORT    5061
#define SIP_MAX_HEADER_SIZE     8192
#define SIP_MAX_BODY_SIZE       65536
#define SIP_MAX_URI_LENGTH      512
#define SIP_MAX_HEADERS         64

/* ============================================
 * SIP Methods
 * ============================================ */

typedef enum {
    SIP_METHOD_UNKNOWN = 0,
    SIP_METHOD_INVITE,
    SIP_METHOD_ACK,
    SIP_METHOD_BYE,
    SIP_METHOD_CANCEL,
    SIP_METHOD_REGISTER,
    SIP_METHOD_OPTIONS,
    SIP_METHOD_PRACK,
    SIP_METHOD_SUBSCRIBE,
    SIP_METHOD_NOTIFY,
    SIP_METHOD_PUBLISH,
    SIP_METHOD_INFO,
    SIP_METHOD_REFER,
    SIP_METHOD_MESSAGE,
    SIP_METHOD_UPDATE
} sip_method_t;

/* ============================================
 * SIP Response Codes
 * ============================================ */

/* 1xx Provisional */
#define SIP_100_TRYING           100
#define SIP_180_RINGING          180
#define SIP_181_FORWARDING       181
#define SIP_182_QUEUED           182
#define SIP_183_PROGRESS         183

/* 2xx Success */
#define SIP_200_OK               200
#define SIP_202_ACCEPTED         202

/* 3xx Redirection */
#define SIP_300_MULTIPLE         300
#define SIP_301_MOVED_PERM       301
#define SIP_302_MOVED_TEMP       302
#define SIP_305_USE_PROXY        305
#define SIP_380_ALTERNATIVE      380

/* 4xx Client Error */
#define SIP_400_BAD_REQUEST      400
#define SIP_401_UNAUTHORIZED     401
#define SIP_403_FORBIDDEN        403
#define SIP_404_NOT_FOUND        404
#define SIP_405_NOT_ALLOWED      405
#define SIP_406_NOT_ACCEPTABLE   406
#define SIP_407_PROXY_AUTH       407
#define SIP_408_TIMEOUT          408
#define SIP_480_UNAVAILABLE      480
#define SIP_481_NO_DIALOG        481
#define SIP_486_BUSY             486
#define SIP_487_TERMINATED       487
#define SIP_488_NOT_ACCEPTABLE   488

/* 5xx Server Error */
#define SIP_500_SERVER_ERROR     500
#define SIP_501_NOT_IMPLEMENTED  501
#define SIP_502_BAD_GATEWAY      502
#define SIP_503_UNAVAILABLE      503
#define SIP_504_TIMEOUT          504

/* 6xx Global Failure */
#define SIP_600_BUSY_EVERYWHERE  600
#define SIP_603_DECLINE          603

/* ============================================
 * SIP URI
 * ============================================ */

typedef enum {
    SIP_URI_SCHEME_SIP,
    SIP_URI_SCHEME_SIPS,
    SIP_URI_SCHEME_TEL
} sip_uri_scheme_t;

typedef struct {
    sip_uri_scheme_t scheme;
    char user[128];
    char password[64];
    char host[256];
    uint16_t port;
    char transport[16];
    char parameters[256];
} sip_uri_t;

/* ============================================
 * SIP Headers
 * ============================================ */

typedef struct {
    char name[64];
    char value[512];
} sip_header_t;

typedef struct {
    char tag[64];
    char display_name[128];
    sip_uri_t uri;
} sip_address_t;

typedef struct {
    char branch[64];
    char transport[16];
    char host[256];
    uint16_t port;
    char received[64];
    int rport;
} sip_via_t;

typedef struct {
    char call_id[128];
} sip_call_id_t;

typedef struct {
    uint32_t seq;
    sip_method_t method;
} sip_cseq_t;

/* ============================================
 * SIP Message
 * ============================================ */

typedef enum {
    SIP_MSG_REQUEST,
    SIP_MSG_RESPONSE
} sip_message_type_t;

typedef struct {
    sip_message_type_t type;
    
    /* Request line (for requests) */
    sip_method_t method;
    sip_uri_t request_uri;
    
    /* Status line (for responses) */
    int status_code;
    char reason_phrase[128];
    
    /* Common headers */
    sip_via_t via[8];
    int via_count;
    sip_address_t from;
    sip_address_t to;
    sip_call_id_t call_id;
    sip_cseq_t cseq;
    sip_address_t contact;
    int max_forwards;
    int expires;
    int content_length;
    char content_type[64];
    
    /* Additional headers */
    sip_header_t headers[SIP_MAX_HEADERS];
    int header_count;
    
    /* Body */
    char *body;
    size_t body_size;
    
    /* Raw message for reference */
    char *raw;
    size_t raw_size;
} sip_message_t;

/* ============================================
 * SIP Message Functions
 * ============================================ */

/**
 * @brief Create empty SIP message
 */
sip_message_t *sip_message_create(void);

/**
 * @brief Destroy SIP message
 */
void sip_message_destroy(sip_message_t *msg);

/**
 * @brief Parse SIP message from buffer
 */
voice_error_t sip_message_parse(const char *buffer, size_t size, sip_message_t *msg);

/**
 * @brief Serialize SIP message to buffer
 */
voice_error_t sip_message_serialize(const sip_message_t *msg, char *buffer, 
                                     size_t buffer_size, size_t *out_size);

/**
 * @brief Create INVITE request
 */
voice_error_t sip_message_create_invite(sip_message_t *msg,
                                         const sip_uri_t *to,
                                         const sip_uri_t *from,
                                         const char *call_id,
                                         uint32_t cseq,
                                         const char *sdp);

/**
 * @brief Create ACK request
 */
voice_error_t sip_message_create_ack(sip_message_t *msg,
                                      const sip_message_t *invite);

/**
 * @brief Create BYE request
 */
voice_error_t sip_message_create_bye(sip_message_t *msg,
                                      const char *call_id,
                                      const sip_uri_t *to,
                                      const sip_uri_t *from,
                                      uint32_t cseq);

/**
 * @brief Create REGISTER request
 */
voice_error_t sip_message_create_register(sip_message_t *msg,
                                           const sip_uri_t *registrar,
                                           const sip_uri_t *aor,
                                           const sip_address_t *contact,
                                           int expires);

/**
 * @brief Create response to request
 */
voice_error_t sip_message_create_response(sip_message_t *response,
                                           const sip_message_t *request,
                                           int status_code,
                                           const char *reason);

/**
 * @brief Add header to message
 */
voice_error_t sip_message_add_header(sip_message_t *msg,
                                      const char *name,
                                      const char *value);

/**
 * @brief Get header value
 */
const char *sip_message_get_header(const sip_message_t *msg, const char *name);

/**
 * @brief Set message body
 */
voice_error_t sip_message_set_body(sip_message_t *msg,
                                    const char *content_type,
                                    const char *body,
                                    size_t body_size);

/* ============================================
 * SIP URI Functions
 * ============================================ */

/**
 * @brief Parse SIP URI
 */
voice_error_t sip_uri_parse(const char *str, sip_uri_t *uri);

/**
 * @brief Format SIP URI to string
 */
voice_error_t sip_uri_to_string(const sip_uri_t *uri, char *buffer, size_t size);

/**
 * @brief Compare two URIs
 */
bool sip_uri_equals(const sip_uri_t *a, const sip_uri_t *b);

/* ============================================
 * Helper Functions
 * ============================================ */

/**
 * @brief Get method name as string
 */
const char *sip_method_to_string(sip_method_t method);

/**
 * @brief Parse method from string
 */
sip_method_t sip_method_from_string(const char *str);

/**
 * @brief Get response reason phrase
 */
const char *sip_status_reason(int status_code);

/**
 * @brief Generate unique Call-ID
 */
void sip_generate_call_id(char *buffer, size_t size, const char *host);

/**
 * @brief Generate unique branch ID
 */
void sip_generate_branch(char *buffer, size_t size);

/**
 * @brief Generate unique tag
 */
void sip_generate_tag(char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_SIP_CORE_H */
