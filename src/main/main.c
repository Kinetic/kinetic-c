#include <stdio.h>
#include "KineticProto.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

int main(int argc, char** argv)
{
    size_t i, j;
    size_t len;
    KineticProto proto;
    KineticProto_Command cmd;
    KineticProto_Header header;

    uint8_t buffer[1024];

    KineticProto_init(&proto);
    KineticProto_command__init(&cmd);
    KineticProto_header__init(&header);

    printf ("Seagate Kinetic Protocol C Client Test Utility\n\n");

    printf("KineticProto:\n");
    printf("  size: %lu\n", sizeof(proto));

    printf("  pre-packed size: %lu\n", KineticProto_get_packed_size(&proto));

    header.clusterversion = 12345678;
    header.has_clusterversion = TRUE;
    header.identity = -12345678;
    header.has_identity = TRUE;
    cmd.header = &header;
    proto.command = &cmd;

    printf("  estimated packed size: %lu\n", KineticProto_get_packed_size(&proto));

    len = KineticProto_pack(&proto, buffer);
    printf("  actual packed size: %lu\n", len);

    assert(KineticProto_get_packed_size(&proto) == len);

    printf("  buffer contents:\n");
    for (i = 0; i < len; i++)
    {
        printf("    ");
        for (j = 0; j < 8; j++, i++)
        {
            if (i < len)
            {
                printf("%02X ", buffer[i]);
            }
        }
        printf("\n");
    }

	return 0;
}
