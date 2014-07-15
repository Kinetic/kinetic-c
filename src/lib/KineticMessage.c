#include "KineticMessage.h"

void KineticMessage_Init(
    KineticMessage* const message,
    KineticExchange* const exchange)
{
    // Initialize protobuf fields
    KineticProto_init(&message->proto);
    KineticProto_command_init(&message->command);
    KineticProto_header_init(&message->header);
    KineticProto_body_init(&message->body);
    KineticProto_status_init(&message->status);

    // Assemble the message
    message->command.header = &message->header;
    message->command.body = &message->body;
    message->command.status = &message->status;
    message->proto.command = &message->command;

    // Configure/associate the message with the exchange
    KineticExchange_ConfigureHeader(exchange, &message->header);
    message->exchange = exchange;
}

void KineticMessage_BuildNoop(KineticMessage* const message)
{
}
