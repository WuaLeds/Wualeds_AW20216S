// Example: Bouncing / Pong — a self-playing Pong with a bouncing ball.
// Build/upload with:  pio run -e pong -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// A self-playing game of Pong on the 6x12 panel. A ball bounces between the
// left and right walls while two AI paddles (top and bottom) slide sideways to
// catch it. Each rally speeds the ball up a little; when a paddle misses, the
// other player scores and the ball is served again from the center. It runs
// forever with no input.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// This example introduces simple game physics and collision handling on the
// matrix. The ball position is tracked with floating-point coordinates so it
// can move smoothly at sub-pixel speed, then rounded to the nearest LED only
// when drawing.
//
// You will practice:
//   - integrating position from velocity each frame (px += vx).
//   - reflecting velocity off walls and paddles (bounce), with a bit of angle
//     added depending on where the ball hits the paddle.
//   - a tiny "AI": moving each paddle toward the ball, capped by a max speed so
//     it can occasionally miss.
//   - clearScreen() + setPixel() + show() to render, paced with millis().

#include <Arduino.h>
#include <SPI.h>
#include "AW20216S.h"

//*********************************************************** */
//***********        Definitions                              */
//*********************************************************** */
// ── Pins ─────────────────────────────────────────────────
#define PIN_SCK  18
#define PIN_MISO 19
#define PIN_MOSI 23

// Chip Select (CS) pin. On ESP32 the VSPI default CS is GPIO 5.
#define CS_PIN 5

// Row and Column definitions for the 6x12 RGB matrix
#define WIDTH_LED_MATRIX 6
#define HEIGHT_LED_MATIX 12

// ── Game tuning ───────────────────────────────────────────
#define FRAME_MS      60    // Milliseconds between frames.
#define PADDLE_W      3     // Paddle width in pixels.
#define PADDLE_SPEED  1     // Max columns a paddle moves per frame (lower = easier to beat).
#define BALL_SPEED    0.45f // Initial vertical ball speed (pixels per frame).
#define SPEEDUP       1.06f // Ball speed-up factor on every paddle hit.
#define MAX_SPEED     1.20f // Cap on the vertical ball speed.

// Pin used only to gather analog noise for the random seed (leave unconnected).
#define SEED_NOISE_PIN 34

// Colors.
#define TOP_R 0
#define TOP_G 255
#define TOP_B 60
#define BOT_R 0
#define BOT_G 120
#define BOT_B 255
#define BALL_R 255
#define BALL_G 255
#define BALL_B 255

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

// ── Game state ────────────────────────────────────────────
float ballX, ballY; // Ball position (sub-pixel).
float velX, velY;   // Ball velocity (pixels per frame).
int   topPaddleX;   // Left edge of the top paddle (row 0).
int   botPaddleX;   // Left edge of the bottom paddle (row HEIGHT-1).
uint16_t scoreTop, scoreBot;

//*********************************************************** */
//***********        Helper functions                         */
//*********************************************************** */

// Round a float coordinate to the nearest LED index.
static inline int iround(float v) { return (int)(v + 0.5f); }

// Keep a value inside [lo, hi].
static inline int clampi(int v, int lo, int hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// Serve the ball from the center with a random horizontal angle.
// dir = -1 sends it up (toward the top paddle), +1 sends it down.
static void serveBall(int dir)
{
  ballX = (WIDTH_LED_MATRIX - 1) / 2.0f;
  ballY = (HEIGHT_LED_MATIX - 1) / 2.0f;
  velX  = (random(0, 2) ? 1 : -1) * (0.2f + random(0, 40) / 100.0f); // -0.6..-0.2 or 0.2..0.6
  velY  = dir * BALL_SPEED;
}

// Move a paddle's left edge toward a target ball column, capped by PADDLE_SPEED.
static int trackPaddle(int paddleX, float target)
{
  const float center = paddleX + (PADDLE_W - 1) / 2.0f;
  if (target > center + 0.5f)      paddleX += PADDLE_SPEED;
  else if (target < center - 0.5f) paddleX -= PADDLE_SPEED;
  return clampi(paddleX, 0, WIDTH_LED_MATRIX - PADDLE_W);
}

// True if column bx lies within the paddle that starts at paddleX.
static bool paddleCovers(int paddleX, int bx)
{
  return (bx >= paddleX) && (bx < paddleX + PADDLE_W);
}

// Bounce the ball off a paddle, adding angle based on the hit offset, and speed
// the rally up a little (clamped to MAX_SPEED).
static void bounceOffPaddle(int paddleX, int newDir)
{
  const float center = paddleX + (PADDLE_W - 1) / 2.0f;
  velX += (ballX - center) * 0.35f;            // glancing hits curve more
  velX = constrain(velX, -1.2f, 1.2f);

  float speed = fabs(velY) * SPEEDUP;
  if (speed > MAX_SPEED) speed = MAX_SPEED;
  velY = newDir * speed;
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S Pong...");
  delay(500);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, CS_PIN);
  delay(50);

  // 1. Initialize the chip
  if (!ledMatrix.begin())
  {
    Serial.println("Error: AW20216S chip not detected.");
    while (1)
      ; // Stop execution if it fails
  }

  Serial.println("Chip started correctly.");

  // 2. Configure global current (Master brightness)
  ledMatrix.setGlobalCurrent(0x40);

  // 3. Set Scaling (White Balance) to maximum.
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);

  // 4. Seed randomness and set up the first serve.
  randomSeed((uint32_t)analogRead(SEED_NOISE_PIN) ^ micros());
  topPaddleX = botPaddleX = (WIDTH_LED_MATRIX - PADDLE_W) / 2;
  scoreTop = scoreBot = 0;
  serveBall(random(0, 2) ? 1 : -1);
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint32_t lastMs = 0; // Timestamp of the last frame.

  // Non-blocking timing: only advance the game every FRAME_MS.
  const uint32_t now = millis();
  if ((now - lastMs) < FRAME_MS)
    return;
  lastMs = now;

  // 1. Move the ball.
  ballX += velX;
  ballY += velY;

  // 2. Bounce off the left/right walls.
  if (ballX <= 0)                    { ballX = 0;                     velX = -velX; }
  if (ballX >= WIDTH_LED_MATRIX - 1) { ballX = WIDTH_LED_MATRIX - 1;  velX = -velX; }

  // 3. Paddles chase the ball.
  topPaddleX = trackPaddle(topPaddleX, ballX);
  botPaddleX = trackPaddle(botPaddleX, ballX);

  // 4. Handle the top edge (top paddle, row 0).
  const int bx = clampi(iround(ballX), 0, WIDTH_LED_MATRIX - 1);
  if (ballY <= 0 && velY < 0)
  {
    if (paddleCovers(topPaddleX, bx))
    {
      ballY = 0;
      bounceOffPaddle(topPaddleX, +1); // send it back down
    }
    else
    {
      scoreBot++;
      Serial.print("Bottom scores!  Top ");
      Serial.print(scoreTop); Serial.print(" - "); Serial.println(scoreBot);
      serveBall(+1);
    }
  }

  // 5. Handle the bottom edge (bottom paddle, last row).
  if (ballY >= HEIGHT_LED_MATIX - 1 && velY > 0)
  {
    if (paddleCovers(botPaddleX, bx))
    {
      ballY = HEIGHT_LED_MATIX - 1;
      bounceOffPaddle(botPaddleX, -1); // send it back up
    }
    else
    {
      scoreTop++;
      Serial.print("Top scores!     Top ");
      Serial.print(scoreTop); Serial.print(" - "); Serial.println(scoreBot);
      serveBall(-1);
    }
  }

  // 6. Render the frame: paddles, then the ball.
  ledMatrix.clearScreen();

  for (int i = 0; i < PADDLE_W; i++)
  {
    ledMatrix.setPixel(topPaddleX + i, 0,                     TOP_R, TOP_G, TOP_B);
    ledMatrix.setPixel(botPaddleX + i, HEIGHT_LED_MATIX - 1,  BOT_R, BOT_G, BOT_B);
  }

  const int by = clampi(iround(ballY), 0, HEIGHT_LED_MATIX - 1);
  ledMatrix.setPixel(bx, by, BALL_R, BALL_G, BALL_B);

  ledMatrix.show();
}
