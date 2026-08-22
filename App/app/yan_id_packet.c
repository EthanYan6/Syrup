/*
 * Syrup Firmware — Yan ID GGM2 packet (ported from Dondji / mangosteen)
 */
#include <string.h>
#include "app/yan_id_packet.h"
#include "driver/crc.h"

uint16_t YAN_PACKET_Crc16(const uint8_t *data, uint16_t len)
{
    return CRC_CalculateEx(data, len, 0xFFFFu);
}

uint8_t YAN_PACKET_BuildYanId(uint8_t *out, uint8_t out_len, const char *from)
{
    uint16_t crc;

    if (!out || out_len < YAN_PKT_WIRE_LEN)
        return 0;
    memset(out, 0, YAN_PKT_WIRE_LEN);
    out[0] = YAN_PKT_MAGIC0;
    out[1] = YAN_PKT_MAGIC1;
    out[2] = YAN_PKT_MAGIC2;
    out[3] = YAN_PKT_MAGIC3;
    out[4] = YAN_PKT_VERSION;
    out[5] = YAN_PKT_TYPE_YAN_ID;
    out[9]  = 1;
    out[10] = 1;
    strncpy((char *)&out[11], from && from[0] ? from : "UVK1", YAN_PKT_CALLSIGN_LEN);
    strncpy((char *)&out[19], YAN_PKT_TO_ALL, YAN_PKT_CALLSIGN_LEN);
    crc = YAN_PACKET_Crc16(out, YAN_PKT_WIRE_LEN - 2u);
    out[YAN_PKT_WIRE_LEN - 2u] = (uint8_t)(crc & 0xFFu);
    out[YAN_PKT_WIRE_LEN - 1u] = (uint8_t)(crc >> 8);
    return YAN_PKT_WIRE_LEN;
}

bool YAN_PACKET_ParseYanId(const uint8_t *data, uint8_t len, YAN_Packet_t *pkt)
{
    uint16_t crc;
    int i;

    if (!data || !pkt || len < YAN_PKT_WIRE_LEN)
        return false;
    if (data[0] != YAN_PKT_MAGIC0 || data[1] != YAN_PKT_MAGIC1 ||
        data[2] != YAN_PKT_MAGIC2 || data[3] != YAN_PKT_MAGIC3)
        return false;
    if (data[4] != YAN_PKT_VERSION || data[5] != YAN_PKT_TYPE_YAN_ID)
        return false;
    crc = (uint16_t)data[YAN_PKT_WIRE_LEN - 2u] |
          ((uint16_t)data[YAN_PKT_WIRE_LEN - 1u] << 8);
    if (crc != YAN_PACKET_Crc16(data, YAN_PKT_WIRE_LEN - 2u))
        return false;

    memset(pkt, 0, sizeof(*pkt));
    pkt->type = data[5];
    memcpy(pkt->from, &data[11], YAN_PKT_CALLSIGN_LEN);
    pkt->from[YAN_PKT_CALLSIGN_LEN] = 0;
    for (i = (int)YAN_PKT_CALLSIGN_LEN - 1; i >= 0; i--) {
        if (pkt->from[i] == 0 || pkt->from[i] == ' ')
            pkt->from[i] = 0;
        else
            break;
    }
    return pkt->from[0] != 0;
}
