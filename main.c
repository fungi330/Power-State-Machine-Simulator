#include <stdio.h>
#include <stdint.h>

// ---------------------------------------------------------
// 定義硬體控制位元 (Bit Definition)
// ---------------------------------------------------------
#define PWR_CPU_BIT    0  // Bit 0: CPU 電源
#define PWR_RAM_BIT    1  // Bit 1: 記憶體電源
#define PWR_SCREEN_BIT 2  // Bit 2: 螢幕電源
#define PWR_FAN_BIT    3  // Bit 3: 風扇電源

// ---------------------------------------------------------
// 定義系統狀態與事件
// ---------------------------------------------------------
typedef enum {
    STATE_S5_OFF, // 關機狀態 (G3/S5)
    STATE_S0_ON,      // 運作狀態 (S0)
    STATE_S3_SLEEP    // 睡眠狀態 (S3)
} SystemState;

typedef enum {
    EVENT_POWER_BTN,  // 按下電源鍵
    EVENT_LID_CLOSE,  // 闔上螢幕
    EVENT_LID_OPEN,   // 打開螢幕
    EVENT_BAT_LOW,    // 電量過低
    EVENT_NONE        // 無事件
} SystemEvent;

typedef struct {
    SystemState current_state;
    uint8_t power_register;
} HardwareStatus;

// ---------------------------------------------------------
// 印出當前硬體電源狀態 (檢驗位元運算結果)
// ---------------------------------------------------------
void print_hardware_status(HardwareStatus *status) {
    uint8_t power_register = status->power_register;
    printf("--- 硬體狀態 [暫存器值: 0x%02X] ---\n", power_register);
    printf("CPU: %s | ",  (power_register & (1 << PWR_CPU_BIT)) ? "ON" : "OFF");
    printf("RAM: %s | ",  (power_register & (1 << PWR_RAM_BIT)) ? "ON" : "OFF");
    printf("螢幕: %s | ", (power_register & (1 << PWR_SCREEN_BIT)) ? "ON" : "OFF");
    printf("風扇: %s\n",  (power_register & (1 << PWR_FAN_BIT)) ? "ON" : "OFF");
    printf("------------------------------------\n\n");
}

// ---------------------------------------------------------
// 電源狀態機 (Finite State Machine)
// ---------------------------------------------------------
SystemState handle_state_machine(HardwareStatus *status, SystemEvent event) {

    switch (status->current_state) {
        
        // --- 當前處於：S5 關機狀態 ---
        case STATE_S5_OFF:
            if (event == EVENT_POWER_BTN) {
                printf("[系統] 接收到電源鍵，準備開機...\n");
                // 將 CPU, RAM, 螢幕, 風扇全部打開
                status->power_register |= (1 << PWR_CPU_BIT) | (1 << PWR_RAM_BIT) | 
                                          (1 << PWR_SCREEN_BIT) | (1 << PWR_FAN_BIT);
                return STATE_S0_ON;
            }
            break;

        // --- 當前處於：S0 運作狀態 ---
        case STATE_S0_ON:
            if (event == EVENT_POWER_BTN || event == EVENT_BAT_LOW) {
                printf("[系統] 觸發關機程序...\n");
                // 將所有電源切斷
                status->power_register = 0x00;
                return STATE_S5_OFF;
            } 
            else if (event == EVENT_LID_CLOSE) {
                printf("[系統] 螢幕闔上，進入 S3 睡眠模式...\n");
                // 關閉 CPU, 螢幕, 風扇
                status->power_register &= ~((1 << PWR_CPU_BIT) | (1 << PWR_SCREEN_BIT) | (1 << PWR_FAN_BIT));
                // 進入 S3 時，RAM 必須保持供電以保存資料
                status->power_register |= (1 << PWR_RAM_BIT);
                return STATE_S3_SLEEP;
            }
            break;

        // --- 當前處於：S3 睡眠狀態 ---
        case STATE_S3_SLEEP:
            if (event == EVENT_LID_OPEN || event == EVENT_POWER_BTN) {
                printf("[系統] 喚醒事件觸發，回到 S0 運作狀態...\n");
                // 恢復 CPU, 螢幕, 風扇供電
                status->power_register |= (1 << PWR_CPU_BIT) | (1 << PWR_SCREEN_BIT) | (1 << PWR_FAN_BIT);
                return STATE_S0_ON;
            }
            else if (event == EVENT_BAT_LOW) {
                printf("[系統] 睡眠時電量耗盡，強制關機保護...\n");
                status->power_register = 0x00; // 切斷 RAM 的電
                return STATE_S5_OFF;
            }
            break;
    }
    
    // 如果沒有匹配的事件，維持原狀態
    return status->current_state;
}

// ---------------------------------------------------------
// 事件輪詢迴圈 (Event Loop)
// ---------------------------------------------------------
int main() {
    SystemState current_state = STATE_S5_OFF;
    uint8_t power_register = 0x00; // 初始全為 0 (全部斷電)
    HardwareStatus status = {current_state, power_register};
    int user_input;

    printf("=== 筆電 EC 電源狀態機模擬器 ===\n");
    print_hardware_status(&status);

    while (1) {
        printf("目前狀態: %s\n", 
               (current_state == STATE_S5_OFF) ? "S5 關機" : 
               (current_state == STATE_S0_ON)  ? "S0 運作中" : "S3 睡眠");
        printf("請輸入事件 (1:按電源鍵, 2:闔上螢幕, 3:打開螢幕, 4:電量過低, 0:退出): ");
        
        if (scanf("%d", &user_input) != 1) break;

        SystemEvent current_event = EVENT_NONE;
        
        // 將使用者輸入對應到事件
        switch (user_input) {
            case 0: return 0;
            case 1: current_event = EVENT_POWER_BTN; break;
            case 2: current_event = EVENT_LID_CLOSE; break;
            case 3: current_event = EVENT_LID_OPEN;  break;
            case 4: current_event = EVENT_BAT_LOW;   break;
            default: printf("無效輸入\n\n"); continue;
        }

        // 驅動狀態機
        current_state = handle_state_machine(&status, current_event);
        
        // 印出操作後的硬體狀態
        print_hardware_status(&status);
    }

    return 0;
}