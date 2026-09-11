#ifndef DTU_H
#define DTU_H

#include <stdint.h>
#include "nanopb/dtu.pb.h"

typedef int (*dtu_message_handler_t)(OneOfMessage *msg, void *user_data);

int dtu_register_message_handler(ChannelType type, pb_size_t which_values,
                                 dtu_message_handler_t handler, void *user_data);
int dtu_unregister_message_handler(ChannelType type, pb_size_t which_values,
                                   dtu_message_handler_t handler, void *user_data);
int dtu_send_data(const uint8_t *data, uint32_t len);
void start_dtu_server(void);

#endif
