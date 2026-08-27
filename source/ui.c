#include"ui.h"
#include"common.h"
#include"data.h"

// Font handles.
static LCDFont *g_bold_font = NULL;
static LCDFont *g_light_font = NULL;

// Image handles.
static LCDBitmap *g_title = NULL;
static LCDBitmap *g_menu = NULL;

// Measure length of string in number of code points.
static int UTF8Length(const char *text)
{
   int length = 0;

   while( *text != '\0' )
   {
      if( LIKELY((*text & 0x80) == 0) )
      {
         length++;
         text++;
      }
      else if( (*text & 0xe0) == 0xc0 )
      {
         length += 2;
         text += 2;
      }
      else if( (*text & 0xf0) == 0xe0 )
      {
         length += 3;
         text += 3;
      }
      else if( (*text & 0xf8) == 0xf0 )
      {
         length += 4;
         text += 4;
      }
      else
      {
         length++;
         text++;
      }
   }
   return length;
}

// Load images and fonts.
void InitUI(PlaydateAPI *pd)
{
   static const char kFontPath1[] = "/System/Fonts/Asheville-Sans-14-Bold.pft";
   static const char kFontPath2[] = "/System/Fonts/Asheville-Sans-14-Light.pft";

   const char *error;
   g_bold_font = pd->graphics->loadFont(kFontPath1, &error);
   assert(g_bold_font != NULL);

   g_light_font = pd->graphics->loadFont(kFontPath2, &error);
   assert(g_light_font != NULL);

   g_title = pd->graphics->loadBitmap("title", &error);
   assert(g_title != NULL);
}

// Draw text with white background.
static void DrawBoxedText(PlaydateAPI *pd,
                          LCDFont *font,
                          const char *text,
                          int x, int y)
{
   const int length = UTF8Length(text);
   const int text_width = pd->graphics->getTextWidth(
      font,
      text,
      length,
      kUTF8Encoding,
      pd->graphics->getTextTracking());

   pd->graphics->fillRect(x, y, text_width + 16, 25, kColorWhite);
   pd->graphics->drawText(text, length, kUTF8Encoding, x + 8, y + 3);
}

// Draw title screen.
void DrawTitleScreen(PlaydateAPI *pd)
{
   // Don't need to clear screen, since title image covers every pixel.
   pd->graphics->drawBitmap(g_title, 0, 0, kBitmapUnflipped);

   pd->graphics->setFont(g_light_font);
   DrawBoxedText(pd, g_light_font, "(c)2026 uguu.org", 255, 217);

   pd->graphics->setFont(g_bold_font);
   DrawBoxedText(pd, g_bold_font, "A: Easy mode   B: Hard mode", 89, 188);
}

// Draw stats for game over screen.
void DrawGameStats(PlaydateAPI *pd, int time_ms, int trades)
{
   // Do not clear screen, since we are drawing over world background.

   pd->graphics->fillRect(0, 0, SCREEN_WIDTH, 40,
                          (LCDColor)kTranslucentWhite[48]);

   const int minutes = (time_ms / 1000) / 60;
   const int seconds = (time_ms / 1000) % 60;
   char *text = NULL;
   const int length = pd->system->formatString(
      &text,
      "Reached goal in %d:%02d after %d trades",
      minutes, seconds, trades);

   pd->graphics->setFont(g_bold_font);
   pd->graphics->setDrawMode(kDrawModeInverted);
   pd->graphics->drawText(text, length, kASCIIEncoding, 12, 12);
   pd->graphics->setDrawMode(kDrawModeCopy);
   pd->graphics->drawText(text, length, kASCIIEncoding, 10, 10);
   pd->system->realloc(text, 0);
}

// Highlight boxes for a single button.
static void DrawButtonState(PlaydateAPI *pd,
                            PDButtons current,
                            PDButtons pushed,
                            PDButtons released,
                            PDButtons key,
                            int y)
{
   if( (current & key) != 0 )
      pd->graphics->fillRect(104, y, 67, 19, kColorXOR);
   if( (pushed & key) != 0 )
      pd->graphics->fillRect(35, y, 62, 19, kColorXOR);
   if( (released & key) != 0 )
      pd->graphics->fillRect(178, y, 73, 19, kColorXOR);
}

// Draw input statuses for input test mode.
void DrawInputStatus(PlaydateAPI *pd)
{
   pd->graphics->setFont(g_light_font);

   // Read and draw analog sensors.
   const float c = pd->system->getCrankAngle();
   float x, y, z;
   pd->system->getAccelerometer(&x, &y, &z);
   char *text = NULL;
   pd->system->formatString(
      &text,
      "Crank = %.3f%s\n"
      "Accelerometer = (%+.3f, %+.3f, %+.3f)\n\n"
      "\xe2\xac\x86   pushed   current   released\n"   // U+2B06 = up.
      "\xe2\xac\x87   pushed   current   released\n"   // U+2B07 = down.
      "\xe2\xac\x85   pushed   current   released\n"   // U+2B05 = left.
      "\xe2\x9e\xa1   pushed   current   released\n"   // U+27A1 = right.
      "\xe2\x92\xb7   pushed   current   released\n"   // U+24B7 = circle B.
      "\xe2\x92\xb6   pushed   current   released",    // U+24B6 = circle A.
      (double)c,
      pd->system->isCrankDocked() ? " (docked)" : "",
      (double)x,
      (double)y,
      (double)z);
   pd->graphics->drawText(text, UTF8Length(text), kUTF8Encoding, 5, 30);
   pd->system->realloc(text, 0);

   // Read and draw button state.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   DrawButtonState(pd, current, pushed, released, kButtonUp, 90);
   DrawButtonState(pd, current, pushed, released, kButtonDown, 110);
   DrawButtonState(pd, current, pushed, released, kButtonLeft, 130);
   DrawButtonState(pd, current, pushed, released, kButtonRight, 150);
   DrawButtonState(pd, current, pushed, released, kButtonB, 170);
   DrawButtonState(pd, current, pushed, released, kButtonA, 190);

   pd->graphics->setFont(g_bold_font);
   static const char kTestTitle[] = "Input test";
   pd->graphics->drawText(kTestTitle, strlen(kTestTitle), kASCIIEncoding, 5, 5);
}

// Common prologue to initialize menu image.
static void BeginSetMenu(PlaydateAPI *pd)
{
   if( g_menu == NULL )
   {
      g_menu = pd->graphics->newBitmap(
         SCREEN_WIDTH, SCREEN_HEIGHT, kColorClear);
      assert(g_menu != NULL);
   }

   pd->graphics->pushContext(g_menu);
   pd->graphics->clear(kColorClear);
   pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                          (LCDColor)kTranslucentWhite[48]);
}

// Common epilogue for initializing menu image.
static void EndSetMenu(PlaydateAPI *pd)
{
   pd->graphics->setFont(g_bold_font);

   // Version information.
   static const char kContact[] = "omoikane@uguu.org";
   pd->graphics->setDrawMode(kDrawModeInverted);
   pd->graphics->drawText(kVersion, strlen(kVersion), kASCIIEncoding, 6, 202);
   pd->graphics->drawText(kContact, strlen(kContact), kASCIIEncoding, 6, 222);
   pd->graphics->setDrawMode(kDrawModeCopy);
   pd->graphics->drawText(kVersion, strlen(kVersion), kASCIIEncoding, 4, 200);
   pd->graphics->drawText(kContact, strlen(kContact), kASCIIEncoding, 4, 220);

   pd->graphics->popContext();
   pd->system->setMenuImage(g_menu, 0);
}

// Update menu image for when we are in between states.
void SetTransitionMenuImage(PlaydateAPI *pd)
{
   BeginSetMenu(pd);
   EndSetMenu(pd);
}

// Update menu image when we are not in any of the game modes.
void SetTitleMenuImage(PlaydateAPI *pd)
{
   BeginSetMenu(pd);
   pd->graphics->setFont(g_light_font);

   // U+24B6 = e2 92 b6 = circle A.
   DrawBoxedText(pd, g_light_font, "\xe2\x92\xb6 easy mode", 4, 3);
   // U+24B7 = e2 92 b7 = circle B.
   DrawBoxedText(pd, g_light_font, "\xe2\x92\xb7 hard mode", 4, 30);

   EndSetMenu(pd);
}

// Update menu image for game in progress state.
void SetGameRunningMenuImage(PlaydateAPI *pd)
{
   BeginSetMenu(pd);
   pd->graphics->setFont(g_light_font);

   // U+24B6 = e2 92 b6 = circle A.
   // U+24B7 = e2 92 b7 = circle B.
   DrawBoxedText(pd, g_light_font, "Near swap sign:", 4, 3);
   DrawBoxedText(pd, g_light_font,
                 "\xe2\x92\xb6 / \xe2\x92\xb7 trade", 4, 30);

   DrawBoxedText(pd, g_light_font, "Away from swap sign:", 4, 60);
   DrawBoxedText(pd, g_light_font,
                 "\xe2\x92\xb6 / \xe2\x92\xb7 rotate item", 4, 87);

   EndSetMenu(pd);
}

// Update menu image for game over state.
void SetGameOverMenuImage(PlaydateAPI *pd)
{
   BeginSetMenu(pd);
   pd->graphics->setFont(g_light_font);

   // U+24B6 = e2 92 b6 = circle A.
   // U+24B7 = e2 92 b7 = circle B.
   DrawBoxedText(pd, g_light_font,
                 "\xe2\x92\xb6 / \xe2\x92\xb7 return to title", 4, 3);

   EndSetMenu(pd);
}
