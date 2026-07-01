#include <cstring>
#include "constants.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_private/usb_phy.h"
#include "tusb.h"
#include "synth.h"
#include "voice.h"

static const char *TAG = "synth";

// ---- PCM5102A I2S GPIOs ------------------------------------------------------
static constexpr gpio_num_t I2S_BCK  = GPIO_NUM_6;
static constexpr gpio_num_t I2S_LRCK = GPIO_NUM_7;
static constexpr gpio_num_t I2S_DOUT = GPIO_NUM_8;

// ---- Note event queue --------------------------------------------------------
struct NoteEvent {
    bool    on;
    uint8_t note;
    uint8_t velocity;
};
static QueueHandle_t s_note_queue;

// ---- I2S ---------------------------------------------------------------------
static i2s_chan_handle_t s_tx;

static void i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
                                                        .gpio_cfg = {
                                                            .mclk = I2S_GPIO_UNUSED,
                                                            .bclk = I2S_BCK,
                                                            .ws   = I2S_LRCK,
                                                            .dout = I2S_DOUT,
                                                            .din  = I2S_GPIO_UNUSED,
                                                            .invert_flags = {
                                                                .mclk_inv = false,
                                                                .bclk_inv = false,
                                                                .ws_inv   = false,
                                                            },
                                                        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    ESP_LOGI(TAG, "I2S started at %.0f Hz", (double)sample_rate);
}

// ---- Audio task --------------------------------------------------------------
static void audio_task(void *)
{
    // Construct synth here — stack jetzt kleiner, da keine Lookup-Tabellen mehr
    auto *synth = new Synth<SimpleVoice<buffer_length>>{sample_rate};
    // auto *synth = new Synth<FMVoice<buffer_length>>{sample_rate};
    ESP_LOGI(TAG, "Synth ready");

    static int16_t buf[buffer_length * 2];

    for (;;) {
        // Drain all pending note events before rendering the next block.
        NoteEvent ev;
        while (xQueueReceive(s_note_queue, &ev, 0) == pdTRUE) {
            if (ev.on) synth->note_on(ev.note, ev.velocity);
            else       synth->note_off(ev.note);
        }

        (*synth)(buf);

        size_t written;
        i2s_channel_write(s_tx, buf, sizeof(buf), &written, portMAX_DELAY);
    }
}

// ---- USB + MIDI task ---------------------------------------------------------
static usb_phy_handle_t s_phy;

static void usb_phy_init(void)
{
    usb_phy_config_t cfg = {
        .controller  = USB_PHY_CTRL_OTG,
        .target      = USB_PHY_TARGET_INT,
        .otg_mode    = USB_OTG_MODE_DEVICE,
        .otg_speed   = USB_PHY_SPEED_FULL,
        .ext_io_conf = NULL,
        .otg_io_conf = NULL,
    };
    ESP_ERROR_CHECK(usb_new_phy(&cfg, &s_phy));
}

static void usb_midi_task(void *)
{
    usb_phy_init();
    tusb_init();
    ESP_LOGI(TAG, "TinyUSB initialised");

    for (;;) {
        tud_task();

        uint8_t packet[4];
        while (tud_midi_available()) {
            if (!tud_midi_packet_read(packet)) break;

            const uint8_t status = packet[1];
            const uint8_t d1     = packet[2];
            const uint8_t d2     = packet[3];

            NoteEvent ev{};
            switch (status & 0xF0u) {
                case 0x90u:
                    ev = {d2 > 0, d1, d2};
                    xQueueSend(s_note_queue, &ev, 0);
                    break;
                case 0x80u:
                    ev = {false, d1, 0};
                    xQueueSend(s_note_queue, &ev, 0);
                    break;
                default:
                    break;
            }
        }

        vTaskDelay(1);
    }
}

// ---- Entry point -------------------------------------------------------------
extern "C" void app_main(void)
{
    s_note_queue = xQueueCreate(32, sizeof(NoteEvent));
    i2s_init();
    // Stack jetzt kleiner (16KB reicht), da keine großen Lookup-Tabellen mehr
    xTaskCreatePinnedToCore(audio_task,    "audio",    16384, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(usb_midi_task, "usb_midi",  4096, NULL,  5, NULL, 0);
}

// ---- USB descriptor callbacks ------------------------------------------------
extern "C" {

    static const tusb_desc_device_t s_desc_device = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = 0x0200,
        .bDeviceClass       = 0x00,
        .bDeviceSubClass    = 0x00,
        .bDeviceProtocol    = 0x00,
        .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
        .idVendor           = 0x303A,
        .idProduct          = 0x4011,
        .bcdDevice          = 0x0100,
        .iManufacturer      = 0x01,
        .iProduct           = 0x02,
        .iSerialNumber      = 0x03,
        .bNumConfigurations = 0x01,
    };

    uint8_t const *tud_descriptor_device_cb(void)
    {
        return (uint8_t const *)&s_desc_device;
    }

    enum { ITF_NUM_AUDIO_CONTROL = 0, ITF_NUM_MIDI_STREAMING, ITF_NUM_TOTAL };
    #define EPNUM_MIDI_OUT   0x01
    #define EPNUM_MIDI_IN    0x81
    #define EP_SIZE          64
    #define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

    static const uint8_t s_desc_configuration[] = {
        TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 100),
        TUD_MIDI_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, 0, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, EP_SIZE),
    };

    uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
    {
        (void)index;
        return s_desc_configuration;
    }

    static uint16_t s_desc_str[32];
    static const char *const s_string_desc[] = {
        (const char[]){0x09, 0x04},
        "Espressif",
        "ESP32-S3 synth",
        "ESP32S3-001",
    };

    uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
    {
        (void)langid;
        size_t chr_count;
        if (index == 0) {
            memcpy(&s_desc_str[1], s_string_desc[0], 2);
            chr_count = 1;
        } else {
            if (index >= sizeof(s_string_desc) / sizeof(s_string_desc[0])) return NULL;
            const char *str = s_string_desc[index];
            chr_count = strlen(str);
            if (chr_count > 31) chr_count = 31;
            for (size_t i = 0; i < chr_count; i++)
                s_desc_str[1 + i] = str[i];
        }
        s_desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
        return s_desc_str;
    }

} // extern "C"
