#include <avr/io.h>
#include <avr/pgmspace.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x78 // 0x3C shifted left by 1 bit for raw TWI write (0x3C << 1)

// Pushbutton Pins (Port D pins 2 to 7)
#define BTN_UP    !( PIND & ( 1 << 2 ) )
#define BTN_LEFT  !( PIND & ( 1 << 6 ) )
#define BTN_RIGHT !( PINC & ( 1 << 3 ) )
#define BTN_DOWN  ( BTN_LEFT & BTN_RIGHT )
#define BTN_TL    !( PINC & ( 1 << 2 ) )
#define BTN_TR    !( PIND & ( 1 << 7 ) )

// 16x16 Bitpacked Map in Flash Memory (PROGMEM)
const uint16_t map16x16[16] PROGMEM = {
  0xFFFF, 0x8001, 0x8F01, 0x8001,
  0x80E1, 0x8021, 0x8021, 0x8001,
  0x8001, 0x83C1, 0x8001, 0x8001,
  0x8781, 0x8001, 0x8001, 0xFFFF
};

// --- Q8.8 Fixed-Point Macros & Types ---
typedef int16_t fixed;
#define INT_TO_FIX(x)  ((fixed)((x) << 8))
#define FIX_TO_INT(x)  ((int8_t)((x) >> 8))
#define MUL_FIX(a, b)  ((fixed)(((int32_t)(a) * (b)) >> 8))
#define DIV_FIX(a, b)  ((fixed)((((int32_t)(a)) << 8) / (b)))

// Precalculated Trigonometric Constants for rotSpeed ~0.06
// cos(0.06) * 256 ≈ 255.5 (Rounded to 255)
// sin(0.06) * 256 ≈ 15.3  (Rounded to 15)
#define COS_ROT 255 
#define SIN_ROT 15  

// Player Vectors (Q8.8 Fixed-Point)
fixed posX = INT_TO_FIX(2) + 128; // 2.5
fixed posY = INT_TO_FIX(2) + 128; // 2.5

// Track absolute angle to prevent matrix drift
#define PI (3.14159265358979f)
float playerAngle = PI/64.f; // rot by half, so angle wont be perfect axis
const float rotSpeed = (PI/32.f); // rot by even

const uint8_t divTable[256] PROGMEM = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xcc, 0xaa, 0x92, 0x80, 0x71, 0x66, 0x5d, 0x55, 0x4e, 0x49, 0x44, 
  0x40, 0x3c, 0x38, 0x35, 0x33, 0x30, 0x2e, 0x2c, 0x2a, 0x28, 0x27, 0x25, 0x24, 0x23, 0x22, 0x21, 
  0x20, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x1a, 0x19, 0x18, 0x18, 0x17, 0x17, 0x16, 0x16, 0x15, 
  0x15, 0x14, 0x14, 0x14, 0x13, 0x13, 0x12, 0x12, 0x12, 0x11, 0x11, 0x11, 0x11, 0x10, 0x10, 0x10, 
  0x10, 0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0c, 
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 
  0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 
  0x09, 0x09, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 
  0x08, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 
  0x07, 0x07, 0x07, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x05, 0x05, 0x05, 0x05, 0x05, 
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 
  0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x04, 0x04, 0x04, 
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
};

// Vectors updated dynamically from playerAngle
fixed dirX = -256; 
fixed dirY = 0;
fixed planeX = 0;
fixed planeY = 169; // 0.66 FOV * 256

// --- Low-Level AVR TWI Hardware Primitives ---

inline void twiInit() {
  TWSR = 0x00; // Prescaler = 1
  TWBR = 12;   // SCL clock = F_CPU / (16 + 2*TWBR*Prescaler) -> ~400kHz @ 16MHz
}

inline void twiStart() {
  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT))); 
}

inline void twiStop() {
  TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
  while (TWCR & (1 << TWSTO)); 
}

inline void twiWrite(uint8_t data) {
  TWDR = data;
  TWCR = (1 << TWINT) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT))); 
}

// --- Asynchronous TWI Pipeline Buffer ---
uint8_t twi_tx_buf[10]; // Address + Control + 8 bytes column data
uint8_t twi_idx = 0;
uint8_t twi_len = 0;

// Non-blocking background hardware transfer poll
inline void twiPoll() {
  if (twi_len > 0 && (TWCR & (1 << TWINT))) {
    if (twi_idx < twi_len) {
      TWDR = twi_tx_buf[twi_idx++];
      TWCR = (1 << TWINT) | (1 << TWEN);
    } else {
      // Transmission complete -> issue STOP condition
      twiStop();
      twi_len = 0; // Bus free
    }
  }
}

// Wait until previous frame column packet completes before scheduling next
inline void twiFlushWait() {
  while (twi_len > 0) {
    twiPoll();
  }
}

// Queue column slice packet and fire start condition asynchronously
void queueColumnAsync(uint64_t mask) {
  twiFlushWait(); // Block briefly if hardware pipeline is still full

  twi_tx_buf[0] = OLED_ADDR;
  twi_tx_buf[1] = 0x40; // Data mode prefix
  
  uint8_t* p = (uint8_t*)&mask;
  for (uint8_t i = 0; i < 8; i++) {
    twi_tx_buf[2 + i] = p[i];
  }

  twi_idx = 2;
  twi_len = 10;

  // Trigger START condition asynchronously
  twiStart();
}

// Send single command byte to SSD1306
void oledCmd(uint8_t cmd) {
  twiStart();
  twiWrite(OLED_ADDR);
  twiWrite(0x00); // Command prefix
  twiWrite(cmd);
  twiStop();
}

void setupOLED() {
  twiInit();
  oledCmd(0xAE); // Display OFF

  // --- POWER & BRIGHTNESS REGISTERS ---
  oledCmd(0x8D); oledCmd(0x14); // ENABLE CHARGE PUMP 
  oledCmd(0x81); oledCmd(0xFF); // SET CONTRAST
  oledCmd(0xD9); oledCmd(0xF1); // SET PRE-CHARGE PERIOD
  oledCmd(0xDB); oledCmd(0x40); // SET VCOMH DESELECT LEVEL

  // --- ADDRESSING REGISTERS ---
  oledCmd(0x20); oledCmd(0x01); // Vertical Addressing Mode
  oledCmd(0x21); oledCmd(0x00); oledCmd(127); // Column Address range
  oledCmd(0x22); oledCmd(0x00); oledCmd(7);   // Page Address range
  
  oledCmd(0xAF); // Display ON
}

inline bool isWall(int8_t x, int8_t y) {
  if (x < 0 || x >= 16 || y < 0 || y >= 16) return true;
  return (pgm_read_word(&(map16x16[y])) & (1 << (15 - x))) != 0;
}

void setupPins() {
  // 1. Configure Port D Pins (PD2, PD6, PD7)
  DDRD  &= ~((1 << 2) | (1 << 6) | (1 << 7)); // Set PD2, PD6, PD7 as INPUT (0)
  PORTD |=  ((1 << 2) | (1 << 6) | (1 << 7)); // Enable Internal Pull-ups (1)

  // 2. Configure Port C Pins (PC2/A2, PC3/A3)
  DDRC  &= ~((1 << 2) | (1 << 3));            // Set PC2, PC3 as INPUT (0)
  PORTC |=  ((1 << 2) | (1 << 3));            // Enable Internal Pull-ups (1)
}

void refreshRotation(){
  float s = sin(playerAngle);
  float c = cos(playerAngle);
  
  dirX = (int16_t)(c * 256.0f);
  dirY = (int16_t)(s * 256.0f);
  
  // Perpendicular plane vector for FOV (0.66 * 256 = 169)
  planeX = (int16_t)(s * 169.0f); 
  planeY = (int16_t)(-c * 169.0f);
}

void setup() {
  setupPins();
  setupOLED();
  refreshRotation();
}


void processInput() {
  const fixed moveSpeed = 38; // 0.15 * 256
  bool turned = false;

  // Translation (Fast Fixed-Point Math)
  if (BTN_UP) {
    fixed nX = posX + MUL_FIX(dirX, moveSpeed);
    fixed nY = posY + MUL_FIX(dirY, moveSpeed);
    if (!isWall(FIX_TO_INT(nX), FIX_TO_INT(posY))) posX = nX;
    if (!isWall(FIX_TO_INT(posX), FIX_TO_INT(nY))) posY = nY;
  }
  if (BTN_DOWN) {
    fixed nX = posX - MUL_FIX(dirX, moveSpeed);
    fixed nY = posY - MUL_FIX(dirY, moveSpeed);
    if (!isWall(FIX_TO_INT(nX), FIX_TO_INT(posY))) posX = nX;
    if (!isWall(FIX_TO_INT(posX), FIX_TO_INT(nY))) posY = nY;
  }
  if (BTN_LEFT) {
    fixed nX = posX - MUL_FIX(planeX, moveSpeed);
    fixed nY = posY - MUL_FIX(planeY, moveSpeed);
    if (!isWall(FIX_TO_INT(nX), FIX_TO_INT(posY))) posX = nX;
    if (!isWall(FIX_TO_INT(posX), FIX_TO_INT(nY))) posY = nY;
  }
  if (BTN_RIGHT) {
    fixed nX = posX + MUL_FIX(planeX, moveSpeed);
    fixed nY = posY + MUL_FIX(planeY, moveSpeed);
    if (!isWall(FIX_TO_INT(nX), FIX_TO_INT(posY))) posX = nX;
    if (!isWall(FIX_TO_INT(posX), FIX_TO_INT(nY))) posY = nY;
  }

  // Rotation Tracking
  if (BTN_TL) { 
    playerAngle += rotSpeed; 
    turned = true; 
  }
  if (BTN_TR) { 
    playerAngle -= rotSpeed; 
    turned = true; 
  }

  // Generate fresh, perfectly normalized vectors only if the angle changed
  if (turned) { refreshRotation(); }
}

void renderRaycast() {
  oledCmd(0x21); oledCmd(0); oledCmd(127);
  oledCmd(0x22); oledCmd(0); oledCmd(7);

  // 512 pixels of virtual skybox wrap-around (512 / 2PI ≈ 81.487)
  int16_t skyOffset = (int16_t)(playerAngle * 81.487f);
  uint16_t lastHitwall = -1; 

  for (int16_t x = 0; x < SCREEN_WIDTH; x++) {
    // Interleave TWI polling loop to keep hardware shifting pixels while computing
    twiPoll();
    // Scale X from [0, 128] mapping to [-256, 256] (-1.0 to +1.0)
    fixed cameraX = (x << 2) - 256; 
    fixed rayDirX = dirX + MUL_FIX(planeX, cameraX);
    fixed rayDirY = dirY + MUL_FIX(planeY, cameraX);

    int8_t mapX = FIX_TO_INT(posX);
    int8_t mapY = FIX_TO_INT(posY);

    // Prevent 16-bit overflow by clamping small rayDir values
    int16_t absRayDirX = abs(rayDirX);
    int16_t absRayDirY = abs(rayDirY);
    
    fixed deltaDistX = (absRayDirX <= 2) ? 32767 : (int16_t)(65536L / absRayDirX);
    fixed deltaDistY = (absRayDirY <= 2) ? 32767 : (int16_t)(65536L / absRayDirY);
    
    fixed sideDistX, sideDistY;
    int8_t stepX, stepY;

    if (rayDirX < 0) {
      stepX = -1;
      sideDistX = MUL_FIX(posX - INT_TO_FIX(mapX), deltaDistX);
    } else {
      stepX = 1;
      sideDistX = MUL_FIX(INT_TO_FIX(mapX + 1) - posX, deltaDistX);
    }
    if (rayDirY < 0) {
      stepY = -1;
      sideDistY = MUL_FIX(posY - INT_TO_FIX(mapY), deltaDistY);
    } else {
      stepY = 1;
      sideDistY = MUL_FIX(INT_TO_FIX(mapY + 1) - posY, deltaDistY);
    }

    uint64_t floorDots = 0;
    // DDA Traversal
    bool hit = false;
    uint8_t side = -1;
    uint16_t hitWall = -1;
    while (!hit) {
      if (sideDistX < sideDistY) {
        sideDistX += deltaDistX;
        mapX += stepX;
        side = 0;
      } else {
        sideDistY += deltaDistY;
        mapY += stepY;
        side = 1;
      }
      if (isWall(mapX, mapY)){
        hitWall = mapX + mapY*256;
        hit = true;
      }else{
        fixed perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
        if (perpWallDist > 256 && perpWallDist < 2048) {
          uint8_t shifts = pgm_read_byte(&divTable[perpWallDist >> 3]);
          if( shifts < 32 ){
            if(!( (perpWallDist > 1024) & ((x&1)^(shifts&1)^1) ) ){
              floorDots |= 1ULL << shifts;
            }
          }
        }
      }
    }
    twiPoll(); // Poll hardware during rendering pipeline
    floorDots <<= SCREEN_HEIGHT / 2;

    // --- STAR GENERATION ---
    uint64_t skyMask = 0;
    // Generate a fast pseudo-random number based on the world angle
    uint16_t starHash = (uint16_t)(x - skyOffset) * 31337 * 0x1F23; 
    // ~6% chance to spawn a star in this column
    if ((starHash*starHash & 0xFF) >= 192) { 
      // Extract a random height between 0 and 31 (Top half of screen)
      uint8_t starY = (starHash >> 8) & 0x1F; 
      skyMask  = (1ULL << 63) >> starY;
      skyMask |= 1ULL << starY;
    }
    
    // Wall Calculations
    fixed perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
    if (perpWallDist < 16) perpWallDist = 16; // Clamp against div-by-zero / extreme proximity

    // Line Height: (H / perpDist) -> Since perpDist is Q8.8, multiply H by 256
    int16_t lineHeight = (SCREEN_HEIGHT << 8) / perpWallDist; 
    int16_t drawStart  = -lineHeight / 2 + SCREEN_HEIGHT / 2;
    int16_t drawEnd    =  lineHeight / 2 + SCREEN_HEIGHT / 2;

    if (drawStart < 0) drawStart = 0;
    if (drawEnd >= SCREEN_HEIGHT) drawEnd = SCREEN_HEIGHT - 1;

    // Bitwise Wall Mask Construction
    uint64_t colMask = 0;
    uint64_t colRefl = 0;
    int8_t length = drawEnd - drawStart + 1;

    // Clip the star perfectly behind the wall
    // (1ULL << drawStart) - 1 creates a mask of 1s from the top of the screen down to the wall edge
    uint64_t solidMask = (~((uint64_t)-1 << (length  )) << drawStart);
    uint64_t solidRefl = (~((uint64_t)-1 << (length*2)) << drawStart);
    skyMask &= ~( solidMask | solidRefl );

    if(lastHitwall == -1) lastHitwall = hitWall;
    if ((length > 0) & ( lastHitwall == hitWall )) {
      colMask = (length >= 64) ? ~0ULL : solidMask;
      colRefl = (length >= 64) ? ~0ULL : solidRefl;
      
      if (side == 1) { 
        colMask &= (x & 1) ? 0xEEEEEEEEEEEEEEEEULL: 0xBBBBBBBBBBBBBBBBULL ;
        colRefl &= (x & 1) ? 0x4444444444444444ULL: 0x1111111111111111ULL ;
      }else{
        colRefl &= (x & 1) ? 0xAAAAAAAAAAAAAAAAULL: 0x5555555555555555ULL ;
      }
      colMask |=  colRefl;
    }
    colMask |= floorDots;
    colMask |= skyMask;   // Drop the clipped stars into the final buffer

    lastHitwall = hitWall;
    // 5. Fire non-blocking transmission to TWI hardware
    //queueColumnAsync(colMask);
    
    // Direct TWI Write (Unrolled 8 Pages)
    uint8_t* p = (uint8_t*)&colMask;

    twiStart();
    twiWrite(OLED_ADDR);
    twiWrite(0x40); 
    
    twiWrite(p[0]); twiWrite(p[1]); twiWrite(p[2]); twiWrite(p[3]);
    twiWrite(p[4]); twiWrite(p[5]); twiWrite(p[6]); twiWrite(p[7]);

    twiStop();
    //*/
  }
  twiFlushWait(); // Ensure final column finishes before next frame logic
}

void loop() {
  processInput();
  renderRaycast();
}