#include "ch_config.h"

#include "ch_service.h"

#include "hb_thread/hb_thread.h"
#include "hb_mutex/hb_mutex.h"
#include "hb_memory/hb_memory.h"

#include "hb_log/hb_log.h"
#include "hb_log_console/hb_log_console.h"
#include "hb_log_file/hb_log_file.h"

#include "hb_json/hb_json.h"
#include "hb_http/hb_http.h"

#include "hb_utils/hb_getopt.h"
#include "hb_utils/hb_clock.h"
#include "hb_utils/hb_date.h"
#include "hb_utils/hb_file.h"
#include "hb_utils/hb_time.h"

#include "evhttp.h"
#include "event2/thread.h"

//////////////////////////////////////////////////////////////////////////
#ifndef CH_GRID_REQUEST_DATA_MAX_SIZE
#define CH_GRID_REQUEST_DATA_MAX_SIZE 102400
#endif
//////////////////////////////////////////////////////////////////////////
typedef struct ch_grid_config_t
{
    char name[32 + 1];
    char token[32 + 1];

    char grid_uri[HB_MAX_URI];
    uint16_t grid_port;

    uint32_t max_thread;
    uint32_t max_record;
    uint64_t max_time;

    char log_file[HB_MAX_PATH];
} ch_grid_config_t;
//////////////////////////////////////////////////////////////////////////
typedef struct ch_grid_process_handle_t
{
    evutil_socket_t * ev_socket;
    hb_mutex_handle_t * mutex_ev_socket;

    const ch_grid_config_t * config;

    hb_thread_handle_t * thread;
    ch_service_t * service;
} ch_grid_process_handle_t;
//////////////////////////////////////////////////////////////////////////
typedef ch_http_code_t( *ch_request_func_t )(const hb_json_handle_t * _json, ch_service_t * _service, char * _response, hb_size_t * _size);
//////////////////////////////////////////////////////////////////////////
typedef struct ch_grid_cmd_inittab_t
{
    const char * name;
    ch_request_func_t request;
} ch_grid_cmd_inittab_t;
//////////////////////////////////////////////////////////////////////////
extern ch_http_code_t ch_grid_request_insert( const hb_json_handle_t * _json, ch_service_t * _service, char * _response, hb_size_t * const _size );
extern ch_http_code_t ch_grid_request_select( const hb_json_handle_t * _json, ch_service_t * _service, char * _response, hb_size_t * const _size );
//////////////////////////////////////////////////////////////////////////
static ch_grid_cmd_inittab_t grid_cmds[] =
{
    {"insert", &ch_grid_request_insert},
    {"select", &ch_grid_request_select},
};
//////////////////////////////////////////////////////////////////////////
static void __ch_grid_request( struct evhttp_request * _request, void * _ud )
{
    const char * host = evhttp_request_get_host( _request );
    HB_UNUSED( host );

    const char * uri = evhttp_request_get_uri( _request );
    HB_UNUSED( uri );

    struct evbuffer * output_buffer = evhttp_request_get_output_buffer( _request );

    if( output_buffer == HB_NULLPTR )
    {
        return;
    }

    enum evhttp_cmd_type command_type = evhttp_request_get_command( _request );

    if( command_type == EVHTTP_REQ_OPTIONS )
    {
        struct evkeyvalq * output_headers = evhttp_request_get_output_headers( _request );

        evhttp_add_header( output_headers, "Access-Control-Allow-Origin", "*" );
        evhttp_add_header( output_headers, "Access-Control-Allow-Headers", "*" );
        evhttp_add_header( output_headers, "Access-Control-Allow-Methods", "POST" );
        evhttp_add_header( output_headers, "Content-Type", "application/json" );

        evhttp_send_reply( _request, HTTP_OK, "", output_buffer );

        return;
    }

    if( hb_http_is_request_json( _request ) == HB_FALSE )
    {
        evhttp_send_reply( _request, HTTP_BADREQUEST, "request must be json", output_buffer );

        return;
    }

    ch_grid_process_handle_t * process = (ch_grid_process_handle_t *)_ud;
    
    ch_http_code_t response_code = CH_HTTP_OK;

    hb_size_t response_data_size = 2;
    char response_data[CH_GRID_REQUEST_DATA_MAX_SIZE] = {'\0'};
    strcpy( response_data, "{}" );

    char token[32 + 1] = {'\0'};
    char cmd_name[8 + 1] = {'\0'};
    int32_t count = sscanf( uri, "/%32[^'/']/%8[^'/']", token, cmd_name );

    if( count != 2 )
    {
        evhttp_send_reply( _request, HTTP_BADREQUEST, "incorrect url", output_buffer );

        return;
    }

    const ch_grid_config_t * config = process->config;

    if( strcmp( token, config->token ) != 0 )
    {
        evhttp_send_reply( _request, HTTP_BADREQUEST, "bad token", output_buffer );

        return;
    }

    ch_service_t * service = process->service;
    
    hb_bool_t cmd_found = HB_FALSE;

    for( ch_grid_cmd_inittab_t
        * cmd_inittab = grid_cmds,
        *cmd_inittab_end = grid_cmds + sizeof( grid_cmds ) / sizeof( grid_cmds[0] );
        cmd_inittab != cmd_inittab_end;
        ++cmd_inittab )
    {
        if( strcmp( cmd_inittab->name, cmd_name ) != 0 )
        {
            continue;
        }

        uint8_t request_pool[HB_DATA_MAX_SIZE];

        hb_json_handle_t * json_handle;
        if( hb_http_get_request_json( _request, request_pool, sizeof( request_pool ), &json_handle ) == HB_FAILURE )
        {
            evhttp_send_reply( _request, HTTP_BADREQUEST, "", output_buffer );

            return;
        }

        response_code = (*cmd_inittab->request)(json_handle, service, response_data, &response_data_size);

        cmd_found = HB_TRUE;

        break;
    }    

    if( cmd_found == HB_FALSE )
    {
        evhttp_send_reply( _request, HTTP_BADMETHOD, "", output_buffer );

        return;
    }

    HB_LOG_MESSAGE_INFO( "grid", "response cmd: %s data: %.*s"
        , cmd_name
        , response_data_size
        , response_data
    );

    if( evbuffer_add( output_buffer, response_data, response_data_size ) != 0 )
    {
        evhttp_send_reply( _request, HTTP_INTERNAL, "", output_buffer );

        return;
    }

    struct evkeyvalq * output_headers = evhttp_request_get_output_headers( _request );

    evhttp_add_header( output_headers, "Access-Control-Allow-Origin", "*" );
    evhttp_add_header( output_headers, "Access-Control-Allow-Headers", "*" );
    evhttp_add_header( output_headers, "Access-Control-Allow-Methods", "POST" );
    evhttp_add_header( output_headers, "Content-Type", "application/json" );

    evhttp_send_reply( _request, response_code, "", output_buffer );
}
//////////////////////////////////////////////////////////////////////////
static void __ch_ev_accept_socket( ch_grid_process_handle_t * _handle, struct evhttp * _server )
{
    if( *_handle->ev_socket == -1 )
    {
        const ch_grid_config_t * config = _handle->config;

        struct evhttp_bound_socket * bound_socket = evhttp_bind_socket_with_handle( _server, config->grid_uri, config->grid_port );

        if( bound_socket == HB_NULLPTR )
        {
            int e = errno;
            const char * err = strerror( errno );

            HB_LOG_MESSAGE_ERROR( "grid", "grid '%s' invalid bind socket '%s:%u' error: %s [%d]"
                , config->name
                , config->grid_uri
                , config->grid_port
                , err
                , e
            );

            return;
        }

        *_handle->ev_socket = evhttp_bound_socket_get_fd( bound_socket );
    }
    else
    {
        evhttp_accept_socket( _server, *_handle->ev_socket );
    }
}
//////////////////////////////////////////////////////////////////////////
static void __ch_ev_thread_base( void * _ud )
{
    HB_UNUSED( _ud );

    ch_grid_process_handle_t * handle = (ch_grid_process_handle_t *)_ud;

    struct event_base * base = event_base_new();
    HB_UNUSED( base );

    struct evhttp * http_server = evhttp_new( base );
    HB_UNUSED( http_server );

    evhttp_set_allowed_methods( http_server, EVHTTP_REQ_POST | EVHTTP_REQ_OPTIONS );

    evhttp_set_gencb( http_server, &__ch_grid_request, handle );

    hb_mutex_lock( handle->mutex_ev_socket );

    __ch_ev_accept_socket( handle, http_server );

    hb_mutex_unlock( handle->mutex_ev_socket );

    event_base_dispatch( base );

    evhttp_free( http_server );
}
//////////////////////////////////////////////////////////////////////////
static void * __ch_memory_alloc( hb_size_t _size, void * _ud )
{
    HB_UNUSED( _ud );

    void * ptr = malloc( _size );

    return ptr;
}
//////////////////////////////////////////////////////////////////////////
static void * __ch_memory_realloc( void * _ptr, hb_size_t _size, void * _ud )
{
    HB_UNUSED( _ud );

    void * ptr = realloc( _ptr, _size );

    return ptr;
}
//////////////////////////////////////////////////////////////////////////
static void __ch_memory_free( const void * _ptr, void * _ud )
{
    HB_UNUSED( _ud );

    free( (void *)_ptr );
}
//////////////////////////////////////////////////////////////////////////
static void __event_log( int _severity, const char * _msg )
{
    switch( _severity )
    {
    case EVENT_LOG_MSG:
        {
            hb_log_message( "libevent", HB_LOG_INFO, HB_NULLPTR, 0, "%s", _msg );
        }break;
    case EVENT_LOG_WARN:
        {
            hb_log_message( "libevent", HB_LOG_WARNING, HB_NULLPTR, 0, "%s", _msg );
        }break;
    case EVENT_LOG_ERR:
        {
            hb_log_message( "libevent", HB_LOG_ERROR, HB_NULLPTR, 0, "%s", _msg );
        }break;
    }
}
//////////////////////////////////////////////////////////////////////////
static void __event_fatal( int err )
{
    HB_LOG_MESSAGE_ERROR( "grid", "libevent fatal error: %d"
        , err
    );
}
//////////////////////////////////////////////////////////////////////////
int main( int _argc, char * _argv[] )
{
    HB_UNUSED( _argc );
    HB_UNUSED( _argv );

    hb_memory_initialize( &__ch_memory_alloc, &__ch_memory_realloc, &__ch_memory_free, HB_NULLPTR );

    hb_log_initialize();

    if( hb_log_console_initialize() == HB_FAILURE )
    {
        return EXIT_FAILURE;
    }

#ifdef HB_PLATFORM_WINDOWS
    const WORD wVersionRequested = MAKEWORD( 2, 2 );

    WSADATA wsaData;
    int32_t err = WSAStartup( wVersionRequested, &wsaData );

    if( err != 0 )
    {
        return EXIT_FAILURE;
    }
#endif

    const char * config_file = HB_NULLPTR;
    hb_getopt( _argc, _argv, "--config", &config_file );

    ch_grid_config_t * config = HB_NEW( ch_grid_config_t );

    config->max_thread = 16;
    config->max_record = 10000;
    config->max_time = HB_TIME_SECONDS_IN_WEEK;
    
    strcpy( config->grid_uri, "127.0.0.1" );
    config->grid_port = 5555;

    strcpy( config->token, "" );

    strcpy( config->name, "hb" );

#ifndef HB_DEBUG
    strcpy( config->log_file, "" );
#else
    hb_date_t date;
    hb_date( &date );

    sprintf( config->log_file, "log_grid_%u_%u_%u_%u_%u_%u.log"
        , date.year
        , date.mon
        , date.mday
        , date.hour
        , date.min
        , date.sec );
#endif

    if( config_file != HB_NULLPTR )
    {
        char config_buffer[HB_DATA_MAX_SIZE];
        hb_size_t config_size;
        if( hb_file_read_text( config_file, config_buffer, HB_DATA_MAX_SIZE, &config_size ) == HB_FAILURE )
        {
            return EXIT_FAILURE;
        }

        uint8_t config_pool[HB_DATA_MAX_SIZE];

        hb_json_handle_t * json_handle;
        if( hb_json_create( config_buffer, config_size, config_pool, HB_DATA_MAX_SIZE, &json_handle ) == HB_FAILURE )
        {
            HB_LOG_MESSAGE_CRITICAL( "grid", "config file '%s' wrong json"
                , config_file
            );

            return EXIT_FAILURE;
        }

        hb_json_copy_field_string( json_handle, "grid_uri", config->grid_uri, sizeof( config->grid_uri ) );
        hb_json_get_field_uint16( json_handle, "grid_port", &config->grid_port );

        hb_json_copy_field_string( json_handle, "token", config->token, sizeof( config->token ) );

        hb_json_get_field_uint32( json_handle, "max_thread", &config->max_thread );
        hb_json_get_field_uint32( json_handle, "max_record", &config->max_record );
        hb_json_get_field_uint64( json_handle, "max_time", &config->max_time );

        hb_json_copy_field_string( json_handle, "name", config->name, sizeof( config->name ) );
        hb_json_copy_field_string( json_handle, "log_file", config->log_file, sizeof( config->log_file ) );
    }

    if( strcmp( config->log_file, "" ) != 0 )
    {
        if( hb_log_file_initialize( config->log_file ) == HB_FAILURE )
        {
            HB_LOG_MESSAGE_WARNING( "grid", "grid '%s' invalid initialize [log] file '%s'"
                , config->name
                , config->log_file
            );
        }
    }

    HB_LOG_MESSAGE_INFO( "grid", "start grid with config:" );
    HB_LOG_MESSAGE_INFO( "grid", "------------------------------------" );
    HB_LOG_MESSAGE_INFO( "grid", "max_thread: %u", config->max_thread );
    HB_LOG_MESSAGE_INFO( "grid", "max_record: %u", config->max_record );
    HB_LOG_MESSAGE_INFO( "grid", "max_time: %llu", config->max_time );
    HB_LOG_MESSAGE_INFO( "grid", "grid_uri: %s", config->grid_uri );
    HB_LOG_MESSAGE_INFO( "grid", "grid_port: %u", config->grid_port );
    HB_LOG_MESSAGE_INFO( "grid", "name: %s", config->name );
    HB_LOG_MESSAGE_INFO( "grid", "------------------------------------" );

    ch_service_t * service;
    if( ch_service_create( &service, config->max_record, config->max_time ) == HB_FAILURE )
    {
        return EXIT_FAILURE;
    }

    event_enable_debug_logging( EVENT_DBG_ALL );
    event_set_log_callback( &__event_log );
    event_set_fatal_callback( &__event_fatal );

    hb_mutex_handle_t * mutex_ev_socket;
    if( hb_mutex_create( &mutex_ev_socket ) == HB_FAILURE )
    {
        return EXIT_FAILURE;
    }

    ch_grid_process_handle_t * process_handles = HB_NEWN( ch_grid_process_handle_t, config->max_thread );

    evutil_socket_t ev_socket = -1;
    for( uint32_t i = 0; i != config->max_thread; ++i )
    {
        ch_grid_process_handle_t * process_handle = process_handles + i;

        process_handle->ev_socket = &ev_socket;
        process_handle->mutex_ev_socket = mutex_ev_socket;

        process_handle->config = config;
        process_handle->service = service;

        process_handle->thread = HB_NULLPTR;

        hb_thread_handle_t * thread;
        if( hb_thread_create( &__ch_ev_thread_base, process_handle, &thread ) == HB_FAILURE )
        {
            HB_LOG_MESSAGE_ERROR( "grid", "grid '%s' invalid create thread"
                , config->name
            );

            return EXIT_FAILURE;
        }

        process_handle->thread = thread;
    }

    HB_LOG_MESSAGE_INFO( "grid", "ready.." );

    HB_LOG_MESSAGE_INFO( "grid", "------------------------------------" );

    for( uint32_t i = 0; i != config->max_thread; ++i )
    {
        ch_grid_process_handle_t * process_handle = process_handles + i;

        if( process_handle->thread != HB_NULLPTR )
        {
            hb_thread_join( process_handle->thread );
        }
    }

    hb_mutex_destroy( mutex_ev_socket );
    mutex_ev_socket = HB_NULLPTR;

    for( uint32_t i = 0; i != config->max_thread; ++i )
    {
        ch_grid_process_handle_t * process_handle = process_handles + i;

        if( process_handle->thread != HB_NULLPTR )
        {
            hb_thread_destroy( process_handle->thread );
        }
    }

    HB_DELETEN( process_handles );

    HB_DELETE( config );

#ifdef HB_PLATFORM_WINDOWS
    WSACleanup();
#endif

    hb_log_console_finalize();

#ifdef HB_DEBUG
    hb_log_file_finalize();
#endif

    hb_log_finalize();

    return EXIT_SUCCESS;
}