#include <zephyr/kernel.h>

#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>

#define MAX_REC_COUNT       1
#define NDEF_MSG_BUF_SIZE   128

/* Text message in English with its language code. */
static const uint8_t en_payload[] = "woof";
static const uint8_t en_code[] = {'e', 'n'};

/* Buffer used to hold an NFC NDEF message. */
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];

static void nfc_callback(
    void *context,
    nfc_t2t_event_t event,
    const uint8_t *data,
    size_t data_length)
{
    ARG_UNUSED(context);
    ARG_UNUSED(data);
    ARG_UNUSED(data_length);

    switch (event) {
    case NFC_T2T_EVENT_FIELD_ON:
        printk("NFC field on\n");
        break;
    case NFC_T2T_EVENT_FIELD_OFF:
        printk("NFC field off\n");
        break;
    default:
        break;
    }
}

/**
 * @brief Function for encoding the NDEF text message.
 */
static int welcome_msg_encode(uint8_t *buffer, uint32_t *len)
{
    int err;

    /* Create NFC NDEF text record description in English */
    NFC_NDEF_TEXT_RECORD_DESC_DEF(
        nfc_en_text_rec,
        UTF_8,
        en_code,
        sizeof(en_code),
        en_payload,
        sizeof(en_payload));

    /* Create NFC NDEF message description, capacity - MAX_REC_COUNT
     * records
     */
    NFC_NDEF_MSG_DEF(nfc_text_msg, MAX_REC_COUNT);

    /* Add text records to NDEF text message */
    err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_text_msg),
                   &NFC_NDEF_TEXT_RECORD_DESC(nfc_en_text_rec));
    __ASSERT(err == 0, "Cannot add first record!");

    err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_text_msg),
                      buffer,
                      len);
    __ASSERT(err == 0, "Cannot encode message!");

    return 0;
}

int init_nfc(void)
{
    printk("Initializing NFC...\n");
    uint32_t len = sizeof(ndef_msg_buf);

    /* Set up NFC */
    int nrfSetupRc = nfc_t2t_setup(nfc_callback, NULL);
    __ASSERT_NO_MSG(nrfSetupRc == 0);

    /* Encode welcome message */
    int encodeRc = welcome_msg_encode(ndef_msg_buf, &len);
    __ASSERT_NO_MSG(encodeRc == 0);

    /* Set created message as the NFC payload */
    int payloadRc = nfc_t2t_payload_set(ndef_msg_buf, len);
    __ASSERT_NO_MSG(payloadRc == 0);

    /* Start sensing NFC field */
    int emulationRc = nfc_t2t_emulation_start();
    __ASSERT_NO_MSG(emulationRc == 0);

    printk("NFC configuration done");
    return 0;
}