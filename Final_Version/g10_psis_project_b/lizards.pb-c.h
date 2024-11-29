/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: lizards.proto */

#ifndef PROTOBUF_C_lizards_2eproto__INCLUDED
#define PROTOBUF_C_lizards_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1000000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1003003 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _RemoteChar RemoteChar;
typedef struct _OkMessage OkMessage;
typedef struct _ScoreMessage ScoreMessage;


/* --- enums --- */

typedef enum _Direction {
  DIRECTION__UP = 0,
  DIRECTION__DOWN = 1,
  DIRECTION__LEFT = 2,
  DIRECTION__RIGHT = 3,
  DIRECTION__NONE = 4
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(DIRECTION)
} Direction;

/* --- messages --- */

struct  _RemoteChar
{
  ProtobufCMessage base;
  /*
   * Since Protobuf does not have a direct representation for uint32_t, using int32 here.
   */
  int32_t msg_type;
  uint32_t id;
  /*
   * String in Protobuf does not enforce a length limit.
   */
  char *ch;
  int32_t nchars;
  /*
   * Protobuf does not support fixed-size arrays, so using repeated field.
   */
  size_t n_direction;
  Direction *direction;
};
#define REMOTE_CHAR__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&remote_char__descriptor) \
    , 0, 0, NULL, 0, 0,NULL }


struct  _OkMessage
{
  ProtobufCMessage base;
  int32_t msg_ok;
};
#define OK_MESSAGE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&ok_message__descriptor) \
    , 0 }


struct  _ScoreMessage
{
  ProtobufCMessage base;
  double my_score;
};
#define SCORE_MESSAGE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&score_message__descriptor) \
    , 0 }


/* RemoteChar methods */
void   remote_char__init
                     (RemoteChar         *message);
size_t remote_char__get_packed_size
                     (const RemoteChar   *message);
size_t remote_char__pack
                     (const RemoteChar   *message,
                      uint8_t             *out);
size_t remote_char__pack_to_buffer
                     (const RemoteChar   *message,
                      ProtobufCBuffer     *buffer);
RemoteChar *
       remote_char__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   remote_char__free_unpacked
                     (RemoteChar *message,
                      ProtobufCAllocator *allocator);
/* OkMessage methods */
void   ok_message__init
                     (OkMessage         *message);
size_t ok_message__get_packed_size
                     (const OkMessage   *message);
size_t ok_message__pack
                     (const OkMessage   *message,
                      uint8_t             *out);
size_t ok_message__pack_to_buffer
                     (const OkMessage   *message,
                      ProtobufCBuffer     *buffer);
OkMessage *
       ok_message__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   ok_message__free_unpacked
                     (OkMessage *message,
                      ProtobufCAllocator *allocator);
/* ScoreMessage methods */
void   score_message__init
                     (ScoreMessage         *message);
size_t score_message__get_packed_size
                     (const ScoreMessage   *message);
size_t score_message__pack
                     (const ScoreMessage   *message,
                      uint8_t             *out);
size_t score_message__pack_to_buffer
                     (const ScoreMessage   *message,
                      ProtobufCBuffer     *buffer);
ScoreMessage *
       score_message__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   score_message__free_unpacked
                     (ScoreMessage *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*RemoteChar_Closure)
                 (const RemoteChar *message,
                  void *closure_data);
typedef void (*OkMessage_Closure)
                 (const OkMessage *message,
                  void *closure_data);
typedef void (*ScoreMessage_Closure)
                 (const ScoreMessage *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCEnumDescriptor    direction__descriptor;
extern const ProtobufCMessageDescriptor remote_char__descriptor;
extern const ProtobufCMessageDescriptor ok_message__descriptor;
extern const ProtobufCMessageDescriptor score_message__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_lizards_2eproto__INCLUDED */
