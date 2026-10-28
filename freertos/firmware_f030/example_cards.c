/* Example only. Supply this file with TP_F030_CARD_CONFIG and replace the
 * UID with the UID of a card used for teaching. UIDs are easy to clone. */
#include "tp_f030_reference_cards.h"
static const tp_door_card cards[] = {
    {{0x01,0x02,0x03,0x04},4}
};
const tp_door_card *const tp_f030_cards=cards;
const size_t tp_f030_card_count=sizeof(cards)/sizeof(cards[0]);
