#include <3ds.h>
#include <citro2d.h>
#include "scene_mainmenu.h"
#include "scene_manager.h"
#include "main.h"
#include <sys/stat.h>
#include <jansson.h>
#include "lodepng.h"
#include <stdio.h>
#include "cwav_shit.h"
#include "sprites.h"
#include <string.h>
#include "qrcodegen.h"
#include "logs.h"
#include <math.h>
#include <stdlib.h>
#include <zint.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include "gifdec.h"  
#include "request.h"

#define BARCODE_PNG_PATH "/3ds/barcode_output/barcode.gif"

C2D_Image kuponkurwa;

#define MAX_CODES 128

extern CWAVInfo cwavList[8];
#define MAX_CODES 128
bool is_koleo = false;
bool title_screen = false;
typedef struct {
    char name[64];        // JSON key (e.g., "dupa")
    char symb_name[32];   // e.g., "CODE128"
    char content[5096];    // e.g., "test123"
	int scale;
	int mask;
	int err;
} BarcodeEntry;
bool isLightTheme = false;
u32 themecolor;
u32 themebgcolor;
typedef struct {
    BarcodeEntry entries[MAX_CODES];
    int count;
} BarcodeList;
bool generated = false;
BarcodeList g_Barcodes;
int g_Selected = 0;
float g_ListY = 0.0f, g_TargetListY = 0.0f;
C2D_TextBuf barcodeBuf;
C2D_Text barcodeTexts[MAX_CODES];
float bounce_scale = 1.0f;
bool bounce_active = false;
bool code_dodaj = false;
int selectedCodeIndex = 0; 
#define MAX_CODES 256 
C2D_Text codeTexts[MAX_CODES];
C2D_TextBuf codeBuf;

float g_ListY2 = 120.0f;
float g_TargetListY2 = 120.0f;
ResponseBuffer get_token = {NULL, 0, false};
ResponseBuffer orders = {NULL, 0, false};
ResponseBuffer order = {NULL, 0, false};
const char* codes[] = {
    "AZTEC", "C25STANDARD", "C25MATRIX", "C25INTER", "C25IATA", "C25LOGIC", "C25IND",
    "CODE39", "EXCODE39", "EAN8", "EAN2ADDON", "EAN5ADDON", "EAN13",
    "GS1_128", "EAN128", "CODABAR", "CODE128", "DPLEIT", "DPIDENT",
    "CODE16K", "CODE49", "CODE93", "FLAT", "DBAR_OMN", "DBAR_LTD", 
    "RSS_EXP", "UPCA", "UPCE", "POSTNET", "MSI", "MSI_PLESSEY", "FIM",
    "LOGMARS", "PHARMA", "PZN", "PHARMA_TWO", "CEPNET", "QRCODE",
    "ISBNX", "RM4SCC", "DATAMATRIX", "EAN14", "CODABLOCKF", "JAPANPOST",
    "RSS14STACK", "RSS14STACK_OMNI", "PDF417", "RSS_EXPSTACK", "ONECODE", "PLESSEY",
    "TELEPEN_NUM", "ITF14", "KIX", "MICROQR"
};


void codesSelectorInit(void) {
    codeBuf = C2D_TextBufNew(2048);
    int codeCount = sizeof(codes)/sizeof(codes[0]);
    for (int i = 0; i < codeCount; i++) {
        char line[300];
        snprintf(line, sizeof(line), "%s", codes[i]);
        C2D_TextParse(&codeTexts[i], codeBuf, line);
        C2D_TextOptimize(&codeTexts[i]);
    }
    g_ListY2 = 120.0f;
    g_TargetListY2 = 120.0f;
}



void addCodeName(char* buffer, size_t bufferSize) {
    buffer[0] = '\0';
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, -1);
    swkbdSetInitialText(&swkbd, buffer);
    swkbdSetHintText(&swkbd, "Enter code name");
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "OK", true); // only OK
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0); 
    SwkbdButton button = swkbdInputText(&swkbd, buffer, bufferSize);

    if (button != SWKBD_BUTTON_NONE) {
        printf("Code name entered: %s\n", buffer);
    } else {
        printf("Code name input canceled\n");
        buffer[0] = '\0';
    }
}


void addCodeContent(char* buffer, size_t bufferSize) {
    buffer[0] = '\0';
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetInitialText(&swkbd, buffer);
    swkbdSetHintText(&swkbd, "Enter code content (alphanumeric)");
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "OK", true); // only OK
    swkbdSetValidation(&swkbd, SWKBD_ANYTHING, 0, 0); 
    SwkbdButton button = swkbdInputText(&swkbd, buffer, bufferSize);

    if (button != SWKBD_BUTTON_NONE) {
        printf("Code content entered: %s\n", buffer);
    } else {
        printf("Code content input canceled\n");
        buffer[0] = '\0';
    }
}
void barcodeSelectorInit(void) {
    barcodeBuf = C2D_TextBufNew(2048);
    for (int i = 0; i < g_Barcodes.count; i++) {
        char line[300];
        snprintf(line, sizeof(line), "%s", g_Barcodes.entries[i].name); 
        C2D_TextParse(&barcodeTexts[i], barcodeBuf, line);
        C2D_TextOptimize(&barcodeTexts[i]);
    }
    g_ListY = 120.0f;
    g_TargetListY = 120.0f;
}
void parseCodes(void) {
    json_error_t error;
    json_t *root = json_load_file("/3ds/CTR_WALLET_CODES.json", 0, &error);
    if (!root || !json_is_object(root)) {
        printf("Failed to parse JSON: %s\n", error.text);
        return;
    }

    const char *key;
    json_t *val;
    g_Barcodes.count = 0;

    json_object_foreach(root, key, val) {
        if (g_Barcodes.count >= MAX_CODES) break;
        if (!json_is_object(val)) continue;

        json_t *symb = json_object_get(val, "symbology");
        json_t *data = json_object_get(val, "data");
		json_t *scale = json_object_get(val, "scale");
		json_t *mask = json_object_get(val, "mask");
		json_t *err = json_object_get(val, "err");
		
        if (!json_is_string(symb) || !json_is_string(data)) continue;

        strncpy(g_Barcodes.entries[g_Barcodes.count].name, key, sizeof(g_Barcodes.entries[0].name) - 1);
        strncpy(g_Barcodes.entries[g_Barcodes.count].symb_name, json_string_value(symb), sizeof(g_Barcodes.entries[0].symb_name) - 1);
        strncpy(g_Barcodes.entries[g_Barcodes.count].content, json_string_value(data), sizeof(g_Barcodes.entries[0].content) - 1);
		if (scale) {
			g_Barcodes.entries[g_Barcodes.count].scale = json_integer_value(scale);
		} else {
			g_Barcodes.entries[g_Barcodes.count].scale = 1;
		}
		if (mask) {
			g_Barcodes.entries[g_Barcodes.count].mask = json_integer_value(mask);
		} 
		if (err) {
			g_Barcodes.entries[g_Barcodes.count].err = json_integer_value(err);
		} 
        g_Barcodes.count++;
    }

    json_decref(root);
}
typedef struct {
    int min, max;
} NumberFilterData;

static SwkbdCallbackResult numberFilter(void* user, const char** ppMessage, const char* text, size_t textlen) {
    NumberFilterData* data = (NumberFilterData*)user;
    if (textlen == 0) {
        *ppMessage = "Input required!";
        return SWKBD_CALLBACK_CONTINUE;
    }
    int val = atoi(text);
    if (val < data->min || val > data->max) {
        static char msg[64];
        snprintf(msg, sizeof(msg), "Enter a number between %d and %d", data->min, data->max);
        *ppMessage = msg;
        return SWKBD_CALLBACK_CONTINUE;
    }
    return SWKBD_CALLBACK_OK;
}

int promptNumber(const char* hint, int min, int max) {
    SwkbdState swkbd;
    char buffer[16] = {0};

    swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, sizeof(buffer)-1);
    swkbdSetHintText(&swkbd, hint);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "OK", true); // only OK
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY, 0, 0);

    SwkbdButton button = swkbdInputText(&swkbd, buffer, sizeof(buffer));
    if (button == SWKBD_BUTTON_NONE) return min; 

    int value = atoi(buffer);
    if (value < min) value = min;
    if (value > max) value = max;
    return value;
}

void promptText(const char* hint, char* buffer, size_t bufferSize) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 1, bufferSize - 1);
    swkbdSetInitialText(&swkbd, buffer);
    swkbdSetHintText(&swkbd, hint);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "OK", true);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);

    SwkbdButton button = swkbdInputText(&swkbd, buffer, bufferSize);
    if (button == SWKBD_BUTTON_NONE) {
        buffer[0] = '\0'; // cancel → empty
    }
}

void deleteCodeFromJSON(const char* filename, const char* codeName) {
    json_error_t error;

    // Load JSON file
    json_t *root = json_load_file(filename, 0, &error);
    if (!root) {
        fprintf(stderr, "Error loading JSON: %s\n", error.text);
        return;
    }

    if (!json_is_object(root)) {
        fprintf(stderr, "JSON root is not an object!\n");
        json_decref(root);
        return;
    }

    // Delete the code by name
    if (json_object_get(root, codeName)) {
        json_object_del(root, codeName);
        log_to_file("Deleted code: %s\n", codeName);

        // Save updated JSON
        if (json_dump_file(root, filename, JSON_INDENT(4)) != 0) {
            fprintf(stderr, "Failed to save JSON file.\n");
        }
    } else {
        log_to_file("Code '%s' not found.\n", codeName);
    }

    json_decref(root);
}

void addCodeToJSON(const char* filename, const char* symbology) {
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);

    if (!root) {
        log_to_file("Error loading JSON: %s\n", error.text);
        root = json_object(); 
    } else if (!json_is_object(root)) {
        log_to_file("JSON root is not an object!\n");
        json_decref(root);
        return;
    }

    char bufName[256];
    char bufContent[5096];
 
    if (strcasecmp(symbology, "KOLEO") != 0) {
     
        addCodeName(bufName, sizeof(bufName));
        if (bufName[0] == '\0') {
            json_decref(root);
            code_dodaj = false;
            return;
        }

        addCodeContent(bufContent, sizeof(bufContent));
        if (bufContent[0] == '\0') {
            json_decref(root);
            code_dodaj = false;
            return;
        }

        
        json_t *codeObj = json_object();
        json_object_set_new(codeObj, "symbology", json_string(symbology));
        json_object_set_new(codeObj, "data", json_string(bufContent));

        if (strcasecmp(symbology, "QRCODE") == 0) {
            int errLevel = promptNumber("Error correction (1-4)", 1, 4);
            int mask = promptNumber("Mask pattern (0-7)", 0, 7);
            json_object_set_new(codeObj, "err", json_integer(errLevel));
            json_object_set_new(codeObj, "mask", json_integer(mask));
            json_object_set_new(codeObj, "scale", json_integer(2));
        }


        
        json_object_set_new(root, bufName, codeObj);

       
        if (json_dump_file(root, filename, JSON_INDENT(4)) != 0) {
            log_to_file("Failed to save JSON file.\n");
        } else {
            log_to_file("Code added successfully!\n");
        }
    } else {
        struct curl_slist *headers = NULL;

        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "X-Koleo-Version: 1");
        headers = curl_slist_append(headers, "X-Koleo-Client: Android-90500");
        headers = curl_slist_append(headers, "User-Agent: okhttp/4.12.0");

        char username[128] = {0};
        char password[128] = {0};

        promptText("Enter username", username, sizeof(username));
        if (username[0] == '\0') {
            printf("Username input canceled\n");
            code_dodaj = false;
            return; // bail out if canceled
        }

        promptText("Enter password", password, sizeof(password));
        if (password[0] == '\0') {
            printf("Password input canceled\n");
            code_dodaj = false;
            return;
        }

        char tokreq[512];
        snprintf(tokreq, sizeof(tokreq),
            "{\"username\":\"%s\",\"password\":\"%s\","
            "\"grant_type\":\"password\","
            "\"client_id\":\"626bf916f7a8782e6c1ba0410b784d142867737a9ccd87def8e705b3a039bfe3\"}",
            username, password);

        refresh_data("https://koleo.pl/api/v2/main/oauth/token", tokreq, headers, &get_token, true);
        json_t *root = json_loads(get_token.data, 0, &error);

        json_t *accesstoken  = json_object_get(root, "access_token");
        const char* accesstoken2 = strdup(json_string_value(accesstoken));
        
        size_t auth_len = strlen("Authorization: ") + strlen(accesstoken2) + 1;
        char *authheader = malloc(auth_len);

        snprintf(authheader, auth_len, "Authorization: %s", accesstoken2);
        headers = curl_slist_append(headers, authheader);
        refresh_data("https://koleo.pl/api/v2/main/orders/active", tokreq, headers, &orders, true);

        
        json_t *orders_root = json_loads(orders.data, 0, &error);
        if (!orders_root || !json_is_array(orders_root)) {
            log_to_file("Failed to parse orders JSON\n");
        } else {
            
            json_t *wallet_root = json_load_file(filename, 0, &error);
            if (!wallet_root) {
                wallet_root = json_object();
            }

            size_t index;
            json_t *order_obj;
            json_array_foreach(orders_root, index, order_obj) {
                json_t *id_val = json_object_get(order_obj, "id");
                if (!json_is_integer(id_val)) continue;

                long long order_id = json_integer_value(id_val);

                char url[256];
                snprintf(url, sizeof(url), "https://koleo.pl/api/v2/main/orders/%lld", order_id);

                if (refresh_data(url, "", headers, &order, false) != 0) {
                    log_to_file("refresh_data failed for %lld\n", order_id);
                    continue;
                }

                json_t *order_json = json_loads(order.data, 0, &error);
                if (!order_json) {
                    log_to_file("Failed to parse refreshed order JSON: %s\n", error.text);
                    continue;
                }

                json_t *tickets = json_object_get(order_json, "tickets");
                if (!json_is_array(tickets)) {
                    json_decref(order_json);
                    continue;
                }

                size_t t_index;
                json_t *ticket;
                json_array_foreach(tickets, t_index, ticket) {
                    json_t *b64 = json_object_get(ticket, "base64_img");
                    if (!json_is_string(b64)) continue;

                    char key[64];
                    
                    snprintf(key, sizeof(key), "Bilet_%lld_%zu", order_id, t_index);

                    json_t *ticket_obj = json_object();
                    json_object_set_new(ticket_obj, "symbology", json_string("KOLEO"));
                    json_object_set(ticket_obj, "data", b64);

                    json_object_set_new(wallet_root, key, ticket_obj);
                }

                json_decref(order_json);
            }

            if (json_dump_file(wallet_root, filename, JSON_INDENT(4)) != 0) {
                log_to_file("Failed to save wallet JSON file\n");
            } else {
                log_to_file("Wallet codes updated successfully!\n");
            }

            json_decref(wallet_root);
            json_decref(orders_root);
        }
    }
    json_decref(root);
    parseCodes();
    barcodeSelectorInit();
    g_Selected = 0;
    code_dodaj = false;
}


void codesSelectorUpdate(u32 kDown) {
    int codeCount = sizeof(codes)/sizeof(codes[0]);

    if (kDown & KEY_DUP && selectedCodeIndex > 0) {
        CWAV* sfxc = cwavList[1].cwav;
        cwavPlay(sfxc, 0, 1);
        selectedCodeIndex--;
        g_TargetListY2 += 20.0f;
    }

    if (kDown & KEY_DDOWN && selectedCodeIndex < codeCount - 1) {
        CWAV* sfxc = cwavList[1].cwav;
        cwavPlay(sfxc, 0, 1);
        selectedCodeIndex++;
        g_TargetListY2 -= 20.0f;
    }

    if (kDown & KEY_DLEFT) {
        if (selectedCodeIndex > 0) {
            CWAV* sfxc = cwavList[1].cwav;
            cwavPlay(sfxc, 0, 1);
            selectedCodeIndex -= 10;
            if (selectedCodeIndex < 0) selectedCodeIndex = 0;
            g_TargetListY2 += 20.0f * 10;
        }
    }

    if (kDown & KEY_DRIGHT) {
        if (selectedCodeIndex < codeCount - 1) {
            CWAV* sfxc = cwavList[1].cwav;
            cwavPlay(sfxc, 0, 1);
            selectedCodeIndex += 10;
            if (selectedCodeIndex >= codeCount) selectedCodeIndex = codeCount - 1;
            g_TargetListY2 -= 20.0f * 10;
        }
    }

    if (kDown & KEY_L) {
        isLightTheme = !isLightTheme;
    }

    if (kDown & KEY_A){
        addCodeToJSON("/3ds/CTR_WALLET_CODES.json", codes[selectedCodeIndex]);
    }


    float maxListY = 100.0f;
    float minListY = -20.0f * (codeCount - 1) + 100;

    if (g_TargetListY2 > maxListY) g_TargetListY2 = maxListY;
    if (g_TargetListY2 < minListY) g_TargetListY2 = minListY;

    g_ListY2 += (g_TargetListY2 - g_ListY2) * 0.25f;

    if (g_ListY2 > maxListY) g_ListY2 = maxListY;
    if (g_ListY2 < minListY) g_ListY2 = minListY;
}


void codesSelectorDraw(float x, float startY) {
    int codeCount = sizeof(codes)/sizeof(codes[0]);
    for (int i = 0; i < codeCount; i++) {
        u32 color;
        if (isLightTheme) {
            color = (i == selectedCodeIndex) ? C2D_Color32(170, 170, 170, 255)
                                            : C2D_Color32(0, 0, 0, 255);
        } else {
            color = (i == selectedCodeIndex) ? C2D_Color32(120, 120, 120, 255)
                                            : C2D_Color32(255, 255, 255, 255);
        }

        C2D_DrawText(&codeTexts[i], C2D_WithColor | C2D_AlignCenter,
                     x, startY + i * 20.0f + g_ListY2,
                     1.0f, 0.7f, 0.7f, color);
    }
}





void barcodeSelectorDraw(float x, float startY) {
    for (int i = 0; i < g_Barcodes.count; i++) {
        u32 color;
        if (isLightTheme) {
            color = (i == g_Selected) ? C2D_Color32(170, 170, 170, 255)   
                                      : C2D_Color32(0, 0, 0, 255);   
        } else {
            color = (i == g_Selected) ? C2D_Color32(120, 120, 120, 255)  
                                      : C2D_Color32(255, 255, 255, 255); 
        }

        C2D_DrawText(&barcodeTexts[i], C2D_WithColor | C2D_AlignCenter,
                     x, startY + i * 20.0f + g_ListY,
                     1.0f, 0.7f, 0.7f, color);
    }
}


static inline u32 next_pow2(u32 n) {
    n--;
    n |= n >> 1; n |= n >> 2; n |= n >> 4;
    n |= n >> 8; n |= n >> 16;
    n++;
    return n;
}

static inline u32 clamp(u32 n, u32 lower, u32 upper) {
    return n < lower ? lower : (n > upper ? upper : n);
}

static u32 rgba_to_abgr(u32 rgba) {
    u8 r = (rgba >> 24) & 0xFF;
    u8 g = (rgba >> 16) & 0xFF;
    u8 b = (rgba >> 8) & 0xFF;
    u8 a = rgba & 0xFF; 
    return (a << 24) | (b << 16) | (g << 8) | r;
}
static int is_base64_char(char c) {
    return (isalnum((unsigned char)c) || c == '+' || c == '/');
}
char* sanitize_base64(const char* src) {
    size_t len = strlen(src);
    char* clean = malloc(len + 4); 
    if (!clean) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = src[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=')
            clean[j++] = c;
    }
    while (j % 4 != 0) clean[j++] = '='; 
    clean[j] = '\0';
    return clean;
}
unsigned char* base64_decode(const char* input, size_t* out_len) {
    static const unsigned char d[] = {
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
        52,53,54,55,56,57,58,59,60,61,64,64,64, 0,64,64,
        64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
        64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64
    };

    size_t len = strlen(input);
    if (len % 4 != 0) return NULL;

    size_t out_size = len / 4 * 3;
    if (input[len - 1] == '=') out_size--;
    if (input[len - 2] == '=') out_size--;

    unsigned char* out = malloc(out_size);
    if (!out) return NULL;

    size_t i, j;
    uint32_t v;
    for (i = 0, j = 0; i < len;) {
        v = d[(unsigned char)input[i++]] << 18;
        v |= d[(unsigned char)input[i++]] << 12;
        v |= d[(unsigned char)input[i++]] << 6;
        v |= d[(unsigned char)input[i++]];

        if (j < out_size) out[j++] = (v >> 16) & 0xFF;
        if (j < out_size) out[j++] = (v >> 8) & 0xFF;
        if (j < out_size) out[j++] = v & 0xFF;
    }

    *out_len = out_size;
    return out;
}

void try_load_base64_image(const char* base64_png_data) {
    if (!base64_png_data || !*base64_png_data) {
        log_to_file("No base64 data provided.");
        return;
    }

	char* clean = sanitize_base64(base64_png_data);
	if (!clean) {
		log_to_file("Failed to sanitize base64.");
		return;
	}

	size_t png_data_len = 0;
	unsigned char* png_data = base64_decode(base64_png_data, &png_data_len);
	free(clean);
    if (!png_data || png_data_len == 0) {
        log_to_file("Base64 decode failed.");
        return;
    }


    unsigned char* decoded = NULL;
    unsigned width = 0, height = 0;
	if (png_data_len >= 8) {
		log_to_file("PNG header: %02X %02X %02X %02X %02X %02X %02X %02X",
			png_data[0], png_data[1], png_data[2], png_data[3],
			png_data[4], png_data[5], png_data[6], png_data[7]);
	}

    unsigned error = lodepng_decode32(&decoded, &width, &height, png_data, png_data_len);
    free(png_data);  

    if (error || !decoded) {
        log_to_file("PNG decode failed: %s", lodepng_error_text(error));
        return;
    }

    unsigned orig_w = width;
    unsigned orig_h = height;


    u32 tex_w = clamp(next_pow2(orig_h), 64, 1024);
    u32 tex_h = clamp(next_pow2(orig_w), 64, 1024);
	if (kuponkurwa.tex) {
		C3D_TexDelete(kuponkurwa.tex);
		free(kuponkurwa.tex);
		kuponkurwa.tex = NULL;
	}

	if (kuponkurwa.subtex) {
		free(kuponkurwa.subtex);
		kuponkurwa.subtex = NULL;
	}
    C3D_Tex* tex = malloc(sizeof(C3D_Tex));
    if (!tex || !C3D_TexInit(tex, tex_w, tex_h, GPU_RGBA8)) {
        log_to_file("Texture creation failed.");
        free(decoded);
        if (tex) free(tex);
        return;
    }

    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
    memset(tex->data, 0, tex_w * tex_h * 4); 

	for (u32 y = 0; y < orig_h; ++y) {
		for (u32 x = 0; x < orig_w; ++x) {
			u32 src_i = (y * orig_w + x) * 4;
			u8 r = decoded[src_i];
			u8 g = decoded[src_i + 1];
			u8 b = decoded[src_i + 3]; 
			u8 a = decoded[src_i + 2];  
			u32 abgr = (a << 24) | (r << 16) | (g << 8) | b;


			u32 new_x = y;
			u32 new_y = x;
			u32 dst_offset = (((new_x >> 3) * (tex_w >> 3) + (new_y >> 3)) << 6) +
							 ((new_y & 1) | ((new_x & 1) << 1) | ((new_y & 2) << 1) |
							  ((new_x & 2) << 2) | ((new_y & 4) << 2) | ((new_x & 4) << 3));

			((u32*)tex->data)[dst_offset] = abgr;
		}
	}


    Tex3DS_SubTexture* subtex = malloc(sizeof(Tex3DS_SubTexture));
    if (!subtex) {
        C3D_TexDelete(tex);
        free(tex);
        free(decoded);
        log_to_file("Failed to allocate subtexture.");
        return;
    }


    subtex->width = orig_h;
    subtex->height = orig_w;
    subtex->left = 0.0f;
    subtex->top = 1.0f;
    subtex->right = (float)orig_h / tex_w;
    subtex->bottom = 1.0f - ((float)orig_w / tex_h);

 
    kuponkurwa.tex = tex;
    kuponkurwa.subtex = subtex;

    free(decoded);
    log_to_file("PNG decoded and image loaded successfully.");
	generated = true;
	bounce_scale = 1.2f;  
	bounce_active = true;
}

int symbology_from_name(const char *name) {
    if (!name) return BARCODE_CODE128; // fallback

    if (strcasecmp(name, "CODE11") == 0) return BARCODE_CODE11;
    if (strcasecmp(name, "C25STANDARD") == 0 || strcasecmp(name, "C25MATRIX") == 0) return BARCODE_C25STANDARD;
    if (strcasecmp(name, "C25INTER") == 0) return BARCODE_C25INTER;
    if (strcasecmp(name, "C25IATA") == 0) return BARCODE_C25IATA;
    if (strcasecmp(name, "C25LOGIC") == 0) return BARCODE_C25LOGIC;
    if (strcasecmp(name, "C25IND") == 0) return BARCODE_C25IND;
    if (strcasecmp(name, "CODE39") == 0) return BARCODE_CODE39;
    if (strcasecmp(name, "EXCODE39") == 0) return BARCODE_EXCODE39;
    if (strcasecmp(name, "EAN8") == 0) return BARCODE_EAN8;
    if (strcasecmp(name, "EAN2ADDON") == 0) return BARCODE_EAN_2ADDON;
    if (strcasecmp(name, "EAN5ADDON") == 0) return BARCODE_EAN_5ADDON;
    if (strcasecmp(name, "EAN13") == 0) return BARCODE_EAN13;
    if (strcasecmp(name, "GS1_128") == 0 || strcasecmp(name, "EAN128") == 0) return BARCODE_GS1_128;
    if (strcasecmp(name, "CODABAR") == 0) return BARCODE_CODABAR;
    if (strcasecmp(name, "CODE128") == 0 || strcasecmp(name, "CODE128AB") == 0 || strcasecmp(name, "CODE128B") == 0) return BARCODE_CODE128;
    if (strcasecmp(name, "DPLEIT") == 0) return BARCODE_DPLEIT;
    if (strcasecmp(name, "DPIDENT") == 0) return BARCODE_DPIDENT;
    if (strcasecmp(name, "CODE16K") == 0) return BARCODE_CODE16K;
    if (strcasecmp(name, "CODE49") == 0) return BARCODE_CODE49;
    if (strcasecmp(name, "CODE93") == 0) return BARCODE_CODE93;
    if (strcasecmp(name, "FLAT") == 0) return BARCODE_FLAT;
    if (strcasecmp(name, "DBAR_OMN") == 0 || strcasecmp(name, "RSS14") == 0) return BARCODE_DBAR_OMN;
    if (strcasecmp(name, "DBAR_LTD") == 0 || strcasecmp(name, "RSS_LTD") == 0) return BARCODE_DBAR_LTD;
    if (strcasecmp(name, "DBAR_EXP") == 0 || strcasecmp(name, "RSS_EXP") == 0) return BARCODE_DBAR_EXP;
    if (strcasecmp(name, "TELEPEN") == 0) return BARCODE_TELEPEN;
    if (strcasecmp(name, "UPCA") == 0) return BARCODE_UPCA;
    if (strcasecmp(name, "UPCE") == 0) return BARCODE_UPCE;
    if (strcasecmp(name, "POSTNET") == 0) return BARCODE_POSTNET;
    if (strcasecmp(name, "MSI") == 0 || strcasecmp(name, "MSI_PLESSEY") == 0) return BARCODE_MSI_PLESSEY;
    if (strcasecmp(name, "FIM") == 0) return BARCODE_FIM;
    if (strcasecmp(name, "LOGMARS") == 0) return BARCODE_LOGMARS;
    if (strcasecmp(name, "PHARMA") == 0) return BARCODE_PHARMA;
    if (strcasecmp(name, "PZN") == 0) return BARCODE_PZN;
    if (strcasecmp(name, "PHARMA_TWO") == 0) return BARCODE_PHARMA_TWO;
    if (strcasecmp(name, "CEPNET") == 0) return BARCODE_CEPNET;
    if (strcasecmp(name, "PDF417") == 0) return BARCODE_PDF417;
    if (strcasecmp(name, "PDF417COMP") == 0 || strcasecmp(name, "PDF417TRUNC") == 0) return BARCODE_PDF417COMP;
    if (strcasecmp(name, "MAXICODE") == 0) return BARCODE_MAXICODE;
    if (strcasecmp(name, "QRCODE") == 0 || strcasecmp(name, "QR") == 0) return BARCODE_QRCODE;
    if (strcasecmp(name, "AUSPOST") == 0) return BARCODE_AUSPOST;
    if (strcasecmp(name, "AUSREPLY") == 0) return BARCODE_AUSREPLY;
    if (strcasecmp(name, "AUSROUTE") == 0) return BARCODE_AUSROUTE;
    if (strcasecmp(name, "AUSREDIRECT") == 0) return BARCODE_AUSREDIRECT;
    if (strcasecmp(name, "ISBNX") == 0) return BARCODE_ISBNX;
    if (strcasecmp(name, "RM4SCC") == 0) return BARCODE_RM4SCC;
    if (strcasecmp(name, "DATAMATRIX") == 0) return BARCODE_DATAMATRIX;
    if (strcasecmp(name, "EAN14") == 0) return BARCODE_EAN14;
    if (strcasecmp(name, "VIN") == 0) return BARCODE_VIN;
    if (strcasecmp(name, "CODABLOCKF") == 0) return BARCODE_CODABLOCKF;
    if (strcasecmp(name, "NVE18") == 0) return BARCODE_NVE18;
    if (strcasecmp(name, "JAPANPOST") == 0) return BARCODE_JAPANPOST;
    if (strcasecmp(name, "KOREAPOST") == 0) return BARCODE_KOREAPOST;
    if (strcasecmp(name, "DBAR_STK") == 0 || strcasecmp(name, "RSS14STACK") == 0) return BARCODE_DBAR_STK;
    if (strcasecmp(name, "DBAR_OMNSTK") == 0 || strcasecmp(name, "RSS14STACK_OMNI") == 0) return BARCODE_DBAR_OMNSTK;
    if (strcasecmp(name, "DBAR_EXPSTK") == 0 || strcasecmp(name, "RSS_EXPSTACK") == 0) return BARCODE_DBAR_EXPSTK;
    if (strcasecmp(name, "PLANET") == 0) return BARCODE_PLANET;
    if (strcasecmp(name, "MICROPDF417") == 0) return BARCODE_MICROPDF417;
    if (strcasecmp(name, "USPS_IMAIL") == 0 || strcasecmp(name, "ONECODE") == 0) return BARCODE_USPS_IMAIL;
    if (strcasecmp(name, "PLESSEY") == 0) return BARCODE_PLESSEY;
    if (strcasecmp(name, "TELEPEN_NUM") == 0) return BARCODE_TELEPEN_NUM;
    if (strcasecmp(name, "ITF14") == 0) return BARCODE_ITF14;
    if (strcasecmp(name, "KIX") == 0) return BARCODE_KIX;
    if (strcasecmp(name, "AZTEC") == 0) return BARCODE_AZTEC;
    if (strcasecmp(name, "DAFT") == 0) return BARCODE_DAFT;
    if (strcasecmp(name, "DPD") == 0) return BARCODE_DPD;
    if (strcasecmp(name, "MICROQR") == 0) return BARCODE_MICROQR;


    return BARCODE_CODE128;
}


int hex_to_bytes(const char* hex, unsigned char* out, int max_len) {
    int len = strlen(hex);
    if (len % 2 != 0) return -1;
    int count = 0;
    for (int i = 0; i < len && count < max_len; i += 2) {
        unsigned int byte;
        if (sscanf(&hex[i], "%2x", &byte) != 1) return -1;
        out[count++] = (unsigned char)byte;
    }
    return count;
}

void barcode_generate_and_load(const char *symbology_name, const char *content, int scale, int mask, int err) {
    if (strcmp(symbology_name, "KOLEO") == 0) {
        try_load_base64_image(content);
        return;
    }

    struct zint_symbol* sym = ZBarcode_Create();
    if (!sym) {
        log_to_file("ZBarcode_Create failed");
        return;
    }

    sym->symbology = symbology_from_name(symbology_name);
    sym->show_hrt = 0;
    sym->scale = scale;
    sym->input_mode = DATA_MODE;  // use 8-bit data
    sym->whitespace_width = 5;
    sym->whitespace_height = 5;
    sym->border_width = 5;

    if (mask >= 0 && strcmp(symbology_name, "QRCODE") == 0) sym->option_3 = ZINT_FULL_MULTIBYTE | (mask + 1) << 8;
    if (err >= 0 && strcmp(symbology_name, "QRCODE") == 0) sym->option_1 = err;
    if (sym->symbology == BARCODE_AZTEC) sym->option_2 = 0;  // minimum safe size

    strcpy(sym->bgcolour, "FFFFFFFF");
    strcpy(sym->fgcolour, "000000FF");

    int ret;
    // if (sym->symbology == BARCODE_AZTEC) {
    //     unsigned char raw[5096];
    //     int raw_len = hex_to_bytes(content, raw, sizeof(raw));
    //     if (raw_len < 0) {
    //         log_to_file("Invalid hex in Aztec content");
    //         ZBarcode_Delete(sym);
    //         return;
    //     }
    //     ret = ZBarcode_Encode_and_Buffer(sym, raw, raw_len, 0);
    // } else {
    //     strncpy((char*)sym->text, content, 5096);
    //     sym->text[5096] = '\0';
    //     ret = ZBarcode_Encode_and_Buffer(sym, sym->text, 0, 0);
    // }
    strncpy((char*)sym->text, content, 5096);
    sym->text[5096] = '\0';
    ret = ZBarcode_Encode_and_Buffer(sym, sym->text, 0, 0);

    if (ret != 0) {
        char full_msg[512];
        snprintf(full_msg, sizeof(full_msg), "ZBarcode_Encode_and_Buffer failed with code %d: %s", ret, sym->errtxt);
        log_to_file("%s", full_msg);
        ZBarcode_Delete(sym);
        return;
    }

    if (sym->bitmap_width > 1024 || sym->bitmap_height > 1024) {
        log_to_file("Barcode too big");
        ZBarcode_Delete(sym);
        return;
    }

    log_to_file("Encoded size: %dx%d\n", sym->bitmap_width, sym->bitmap_height);
    u32 tex_w = clamp(next_pow2(sym->bitmap_width), 64, 1024);
    u32 tex_h = clamp(next_pow2(sym->bitmap_height), 64, 1024);

    if (kuponkurwa.tex) {
        C3D_TexDelete(kuponkurwa.tex);
        free(kuponkurwa.tex);
        kuponkurwa.tex = NULL;
    }
    if (kuponkurwa.subtex) {
        free(kuponkurwa.subtex);
        kuponkurwa.subtex = NULL;
    }

    C3D_Tex* tex = malloc(sizeof(C3D_Tex));
    if (!tex || !C3D_TexInit(tex, tex_w, tex_h, GPU_RGBA8)) {
        log_to_file("Texture creation failed");
        free(tex);
        ZBarcode_Delete(sym);
        return;
    }

    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
    memset(tex->data, 0, tex_w * tex_h * 4);

    u32* dst = (u32*)tex->data;
    u8* src = (u8*)sym->bitmap;
    u32 orig_w = sym->bitmap_width;
    u32 orig_h = sym->bitmap_height;

    for (u32 y = 0; y < orig_h; ++y) {
        for (u32 x = 0; x < orig_w; ++x) {
            u32 src_i = (y * orig_w + x) * 3;
            u8 r = src[src_i];
            u8 g = src[src_i + 1];
            u8 b = 0xFF;
            u8 a = src[src_i + 2];

            u32 abgr = (a << 24) | (r << 16) | (g << 8) | b;
            u32 new_x = y;
            u32 new_y = x;
            u32 dst_offset = (((new_x >> 3) * (tex_w >> 3) + (new_y >> 3)) << 6) +
                             ((new_y & 1) | ((new_x & 1) << 1) | ((new_y & 2) << 1) |
                              ((new_x & 2) << 2) | ((new_y & 4) << 2) | ((new_x & 4) << 3));

            dst[dst_offset] = abgr;
        }
    }

    Tex3DS_SubTexture* subtex = malloc(sizeof(Tex3DS_SubTexture));
    subtex->width  = sym->bitmap_width;
    subtex->height = sym->bitmap_height;
    subtex->left   = 0.0f;
    subtex->top    = 1.0f;
    subtex->right  = (float)sym->bitmap_width / tex_w;
    subtex->bottom = 1.0f - ((float)sym->bitmap_height / tex_h);

    kuponkurwa.tex = tex;
    kuponkurwa.subtex = subtex;

    ZBarcode_Delete(sym);
    log_to_file("Barcode rendered into memory and uploaded to GPU");
    generated = true;
    bounce_scale = 1.2f;
    bounce_active = true;
}
void update_bounce_scale(void) {
    if (bounce_active) {
        
        const float speed = 0.2f;  // higher = faster bounce
        bounce_scale += (1.0f - bounce_scale) * speed;


        if (fabsf(bounce_scale - 1.0f) < 0.01f) {
            bounce_scale = 1.0f;
            bounce_active = false;
        }
    }
}
void barcodeSelectorUpdate(u32 kDown) {
    if (kDown & KEY_DUP && g_Selected > 0) {
		CWAV* sfxc = cwavList[1].cwav;
		cwavPlay(sfxc, 0, 1);
        g_Selected--;
        g_TargetListY += 20.0f;
    }
	
    if (kDown & KEY_DDOWN && g_Selected < g_Barcodes.count - 1) {
		CWAV* sfxc = cwavList[1].cwav;
		cwavPlay(sfxc, 0, 1);
        g_Selected++;
        g_TargetListY -= 20.0f;
    }

	if (kDown & KEY_DLEFT) {
		if (g_Selected > 0) {
			CWAV* sfxc = cwavList[1].cwav;
			cwavPlay(sfxc, 0, 1);
			g_Selected -= 10;
			if (g_Selected < 0) g_Selected = 0;
			g_TargetListY += 20.0f * 10;
		}
	}
	if (kDown & KEY_L) {
		isLightTheme = !isLightTheme;
	}

	if (kDown & KEY_DRIGHT) {
		if (g_Selected < g_Barcodes.count - 1) {
			CWAV* sfxc = cwavList[1].cwav;
			cwavPlay(sfxc, 0, 1);
			g_Selected += 10;
			if (g_Selected >= g_Barcodes.count) g_Selected = g_Barcodes.count - 1;
			g_TargetListY -= 20.0f * 10;
		}
	}
    if (kDown & KEY_A) {
		CWAV* sfxc = cwavList[2].cwav;
		cwavPlay(sfxc, 0, 1);
        BarcodeEntry *entry = &g_Barcodes.entries[g_Selected];
        barcode_generate_and_load(entry->symb_name, entry->content, entry->scale, entry->mask, entry->err);
		if (strcmp(entry->symb_name, "KOLEO") == 0) {
			is_koleo = true;
		} else {
			is_koleo = false;
		}
    }
    if (kDown & KEY_X) {
		CWAV* sfxc = cwavList[2].cwav;
		cwavPlay(sfxc, 0, 1);
        BarcodeEntry *entry = &g_Barcodes.entries[g_Selected];
        deleteCodeFromJSON("/3ds/CTR_WALLET_CODES.json", entry->name);
        parseCodes();
        barcodeSelectorInit();
        g_Selected = 0;
    }

	float maxListY = 100.0f; // top of the list
	float minListY = -20.0f * (g_Barcodes.count - 1) + 100; // scroll down to last item


	if (g_TargetListY > maxListY) g_TargetListY = maxListY;
	if (g_TargetListY < minListY) g_TargetListY = minListY;


	g_ListY += (g_TargetListY - g_ListY) * 0.25f;

	if (g_ListY > maxListY) g_ListY = maxListY;
	if (g_ListY < minListY) g_ListY = minListY;
}
#define NUM_STARS 1000
#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 240
#define TOP_WIDTH 400
#define BOTTOM_WIDTH 320
#define SCREEN_HEIGHT 240
#define FULL_HEIGHT (SCREEN_HEIGHT * 2)
#define BOTTOM_X_OFFSET ((TOP_WIDTH - BOTTOM_WIDTH) / 2)  

typedef struct {
    float x, y_base;  
    float speed;
} Star;

Star stars[NUM_STARS]; 
#define LIST_ITEM_HEIGHT 20.0f
#define STARFIELD_EXTRA 120.0f  
void stars_draw_top(void) {
    for (int i = 0; i < NUM_STARS; i++) {
        if (!code_dodaj){
            float y = stars[i].y_base + g_ListY - SCREEN_HEIGHT;

            if (y >= 0.0f && y < SCREEN_HEIGHT) {
                C2D_DrawRectSolid(stars[i].x, y, 0.3f, 2.0f, 2.0f, themecolor);
            }
        } else {
            float y = stars[i].y_base + g_ListY2 - SCREEN_HEIGHT;

            if (y >= 0.0f && y < SCREEN_HEIGHT) {
                C2D_DrawRectSolid(stars[i].x, y, 0.3f, 2.0f, 2.0f, themecolor);
            }
        }
    }
}


void stars_draw_bottom(void) {
    for (int i = 0; i < NUM_STARS; i++) {

        if (!code_dodaj){
            float y = stars[i].y_base + g_ListY;
            float x = stars[i].x - BOTTOM_X_OFFSET;

            if (x >= 0 && x < BOTTOM_WIDTH && y >= 0.0f && y < SCREEN_HEIGHT) {
                C2D_DrawRectSolid(x, y, 0.3f, 2.0f, 2.0f, themecolor);
            }
        } else {
            float y = stars[i].y_base + g_ListY2;
            float x = stars[i].x - BOTTOM_X_OFFSET;

            if (x >= 0 && x < BOTTOM_WIDTH && y >= 0.0f && y < SCREEN_HEIGHT) {
                C2D_DrawRectSolid(x, y, 0.3f, 2.0f, 2.0f, themecolor);
            }
        }
    }
}

void stars_init(void) {
    float virtual_height = 255 * 20.0f + 240.0f * 2.0f;

    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = (float)(rand() % 400);
        stars[i].y_base = -240.0f + ((float)rand() / RAND_MAX) * (virtual_height + 480.0f);  
        stars[i].speed = 0.8f + ((float)(rand() % 100) / 100.0f) * 2.0f; 
    }
}
void stars_update(void) {
    float virtual_height = 255 * 20.0f + 240.0f * 2.0f;

    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].y_base -= stars[i].speed;

        if (stars[i].y_base < -240.0f) {
            stars[i].y_base = virtual_height + 240.0f;
            stars[i].x = (float)(rand() % 400);
            stars[i].speed = 0.8f + ((float)(rand() % 100) / 100.0f) * 2.0f;
        }
    }
}




bool isFlashing;

C2D_TextBuf MainMenu;
C2D_Text menu_Text[100];

static float fade_alpha = 255.0f; 
static float fade_timer = 0.0f;
static bool fading_out = false; 
static float easeOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}


static float fly_time = 0.0f;
static float base_x = 0.0f;    
static float base_y = 0.0f;    


static float amp_y = 10.0f;    
static float amp_x = 6.0f;    
static float speed_y = 2.0f;   
static float speed_x = 1.2f;   


#define TRAIL_COUNT 6
static float trail_delay = 0.05f; 

void sceneMainMenuInit(void) {
    MainMenu = C2D_TextBufNew(2048);
    C2D_TextBufClear(MainMenu);
    C2D_TextParse(&menu_Text[0], MainMenu, "Press A");
    C2D_TextOptimize(&menu_Text[0]);
    C2D_TextParse(&menu_Text[1], MainMenu, "Select Code Type");
    C2D_TextOptimize(&menu_Text[1]);
    C2D_TextParse(&menu_Text[2], MainMenu, "Y - Add a new code");
    C2D_TextOptimize(&menu_Text[2]);
    C2D_TextParse(&menu_Text[3], MainMenu, "B - Go Back");
    C2D_TextOptimize(&menu_Text[3]);
    C2D_TextParse(&menu_Text[4], MainMenu, "X - Delete the code");
    C2D_TextOptimize(&menu_Text[4]);
	parseCodes();
	stars_init();
    codesSelectorInit();
    isFlashing = false;
    code_dodaj = false;
	CWAV* bgm = cwavList[0].cwav;
    bgm->volume = 0.62f;
	cwavPlay(bgm, 0, 1);
    title_screen = false;
}

void sceneMainMenuUpdate(uint32_t kDown, uint32_t kHeld) {
    fly_time += 1.0f / 60.0f; 

    if (title_screen && fading_out) {
        fade_timer += 1.0f / 60.0f; 
        float t = fade_timer / 0.6f;
        if (t >= 1.0f) {
            t = 1.0f;
            fading_out = false;
        }
        fade_alpha = 255.0f * (1.0f - easeOutCubic(t));
    }
    if (title_screen && !code_dodaj) {
	    barcodeSelectorUpdate(kDown);
    } 
    if (code_dodaj){
        codesSelectorUpdate(kDown);
    }
    if (kDown & KEY_Y) {
        code_dodaj = true;
    }
    if (kDown & KEY_B && code_dodaj) {
        code_dodaj = false;
    }
    if (kDown & KEY_A && !title_screen) {
		CWAV* sfxc = cwavList[2].cwav;
		cwavPlay(sfxc, 0, 1);
        barcodeSelectorInit();
        title_screen = true;
       
        fading_out = true;
        fade_alpha = 255.0f;
        fade_timer = 0.0f;
    }
	stars_update();
	update_bounce_scale();
	if (isLightTheme) {
		themecolor = C2D_Color32(0, 0, 0, 255);
		themebgcolor = C2D_Color32(255, 255, 255, 255);
	} else {
		themecolor = C2D_Color32(255, 255, 255, 255);
		themebgcolor = C2D_Color32(48, 51, 56, 255);
	}
}
void sceneMainMenuRender(void) {

    C2D_SceneBegin(top);
    C2D_TargetClear(top, themebgcolor); 
    stars_draw_top();

    if (generated && !code_dodaj) {
        const float screen_width = 400.0f;
        const float screen_height = 240.0f;

        float base_scale = is_koleo ? 0.4f : 1.0f;
        float scale = base_scale * bounce_scale;

        float img_width  = kuponkurwa.subtex->width  * scale;
        float img_height = kuponkurwa.subtex->height * scale;

        float draw_x = floorf((screen_width  - img_width)  / 2.0f + 0.4f);
        float draw_y = floorf((screen_height - img_height) / 2.0f + 0.4f);

        float border_thickness = 5.0f;

       
        const float shadow_offset_x = is_koleo ? 3.0f : 5.0f;
        const float shadow_offset_y = is_koleo ? 3.0f : 5.0f;

       
        float shadow_width  = img_width;
        float shadow_height = img_height;
        if (is_koleo) {
            
            shadow_width  *= 1.05f;
            shadow_height *= 1.05f;
        }

      
        C2D_DrawRectSolid(
            draw_x + shadow_offset_x,
            draw_y + shadow_offset_y,
            0.4f,
            shadow_width,
            shadow_height,
            C2D_Color32(0, 0, 0, 128)  
        );
		
        if (is_koleo) {
            C2D_DrawRectSolid(
                draw_x - border_thickness, 
                draw_y - border_thickness, 
                0.4f, 
                img_width + 2 * border_thickness, 
                img_height + 2 * border_thickness, 
                C2D_Color32(255, 255, 255, 255) 
            );
        }



        C2D_DrawImageAt(kuponkurwa, draw_x, draw_y, 0.5f, NULL, scale, scale);
    }
    if (code_dodaj) {
        C2D_DrawText(&menu_Text[1], C2D_WithColor | C2D_AlignCenter, 200, 100, 1.0f, 1.0f, 1.0f, themecolor);
        C2D_DrawText(&menu_Text[3], C2D_WithColor | C2D_AlignRight, 390, 220, 1.0f, 0.5f, 0.5f, themecolor);
    } else if (title_screen) {
        C2D_DrawText(&menu_Text[2], C2D_WithColor | C2D_AlignRight, 390, 220, 1.0f, 0.5f, 0.5f, themecolor);
        C2D_DrawText(&menu_Text[4], C2D_WithColor | C2D_AlignRight, 390, 200, 1.0f, 0.5f, 0.5f, themecolor);
    }
    if (fade_alpha > 0.0f && title_screen) {
        C2D_DrawRectSolid(0, 0, 1.0f, 400, 240, C2D_Color32(255, 255, 255, (u8)fade_alpha));
    }
    if (!title_screen){
        for (int i = TRAIL_COUNT; i > 0; i--) {
            float t_offset = fly_time - i * trail_delay;

            float swing_y = sinf(t_offset * speed_y) * amp_y;
            float swing_x = cosf(t_offset * speed_x) * amp_x;

            float draw_x = base_x + swing_x;
            float draw_y = base_y + swing_y;

            float alpha_f = 1.0f - (float)i / (TRAIL_COUNT + 1);
            u8 alpha = (u8)(alpha_f * 255);

          
            u8 r = (u8)(128 + 127 * sinf(t_offset * 2.0f));
            u8 g = (u8)(128 + 127 * sinf(t_offset * 2.0f + 2.094f));
            u8 b = (u8)(128 + 127 * sinf(t_offset * 2.0f + 4.188f));

        
            C2D_ImageTint tint;
            C2D_PlainImageTint(&tint, C2D_Color32(r, g, b, alpha), 1.0f);

            C2D_DrawImageAt(flyinglogo, draw_x, draw_y, 0.0f, &tint, 1.0f, 1.0f);
        }

 
        float swing_y = sinf(fly_time * speed_y) * amp_y;
        float swing_x = cosf(fly_time * speed_x) * amp_x;
        C2D_DrawImageAt(flyinglogo, base_x + swing_x, base_y + swing_y, 0.0f, NULL, 1.0f, 1.0f);

    }

    C2D_TargetClear(bottom, themebgcolor);
    C2D_SceneBegin(bottom);
    stars_draw_bottom();

    if (title_screen){
        if (!code_dodaj) {
            barcodeSelectorDraw(160.0f, 0.0f);
        } else {
            codesSelectorDraw(160.0f, 0.0f);
        }
    } else {
        C2D_DrawText(&menu_Text[0], C2D_WithColor | C2D_AlignCenter, 160, 100, 1.0f, 1.0f, 1.0f, themecolor);
    }
    if (fade_alpha > 0.0f && title_screen) {
        C2D_DrawRectSolid(0, 0, 1.0f, 400, 240, C2D_Color32(255, 255, 255, (u8)fade_alpha));
    }
}
void sceneMainMenuExit(void) {
  
}
