
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <sys/param.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <libgen.h>

#include "Prototypes.h"

#include "config.h"
#include "debug.h"


//#link m

void printVersionFlag() {
   clear();
   printf("dit " VERSION " - (C) 2005-2006 Hisham Muhammad.\n");
   printf("Released under the GNU GPL.\n\n");
   exit(0);
}

void clearStatusBar() {
   int y, x;
   getyx(stdscr, y, x);
   attrset(CRT_colors[StatusColor]);
   mvhline(LINES - 1, 0, ' ', COLS);
   move(y, x);
}

void statusMessage(char* message) {
   int cursorX, cursorY;
   getyx(stdscr, cursorY, cursorX);
   attrset(CRT_colors[StatusColor]);
   mvhline(LINES - 1, 0, ' ', COLS);
   mvprintw(LINES - 1, 0, message);
   attrset(CRT_colors[NormalColor]);
   move(cursorY, cursorX);
}

bool attemptSave(Buffer* buffer) {
   Field* saveAsField = Field_new("Save failed. Save as:", 0, LINES-1, COLS-2);
   bool saved = false;
   while (true) {
      saved = Buffer_save(buffer);
      if (!saved) {
         Field_setValue(saveAsField, buffer->fileName);
         bool quitMask[255] = {0};
         int ch = Field_quickRun(saveAsField, quitMask);
         if (ch == 13) {
            free(buffer->fileName);
            buffer->fileName = Field_getValue(saveAsField);
         } else
            break;
      } else
         break;
   }
   Field_delete(saveAsField);
   return saved;
}

inline void restoreRecentHistory(TabManager* tabs, int items) {
   char fileName[4097];
   snprintf(fileName, 4096, "%s/.dit/recent", getenv("HOME"));
   fileName[4095] = '\0';
   FILE* fd = fopen(fileName, "r");
   if (fd) {
      char line[256];
      while (!feof(fd)) {
         int ok = fscanf(fd, "%255[^\n]\n", line);
         if (ok && !TabManager_find(tabs, line)) {
            TabManager_add(tabs, line, basename(line), NULL);
            items++;
            if (items == 5)
               break;
         }
      }
      fclose(fd);
   }
}

inline void storeRecentHistory(TabManager* tabs) {
   char fileName[4097];
   snprintf(fileName, 4096, "%s/.dit/recent", getenv("HOME"));
   fileName[4095] = '\0';
   FILE* fd = fopen(fileName, "w");
   if (fd) {
      int items = Vector_size(tabs->items);
      for (int i = 0; i < 5 && i < items; i++) {
         TabPage* page = (TabPage*) Vector_get(tabs->items, i);
         fprintf(fd, "%s\n", page->name);
      }
      fclose(fd);
   }
}

int main(int argc, char** argv) {

   int jump = 1;
   
   if (argc > 1) {
      if (String_eq(argv[1], "--version")) {
         printVersionFlag();
      } else if (argv[1][0] == '+') {
         argv[1]++;
         jump = atoi(argv[1]);
         argv++;
      }
   } else {
      fprintf(stderr, "Usage: dit <filename>\n");
      exit(0);
   }

   int quit = 0;
   
   struct stat st;
   stat(argv[1], &st);
   if (S_ISDIR(st.st_mode)) {
      fprintf(stderr, "dit: %s is a directory.\n", argv[1]);
      exit(0);
   }
   char dir[1000];
   realpath(argv[1], dir);
   dirname(dir);
   bool exists = (access(argv[1], F_OK) == 0);
   bool canWriteDir = (access(dir, W_OK) == 0);
   bool canWrite = (access(argv[1], W_OK) == 0);
   if ((exists && !canWrite) || (!exists && !canWriteDir)) {
      char buffer[4096];
      snprintf(buffer, 4095, "sudo %s %s", argv[0], argv[1]);
      int ret = system(buffer);
      if (ret == 0)
         exit(0);
   }
   CRT_init();
   Script_init();
   
   TabManager* tabs = TabManager_new(0, 0, COLS, LINES, 20);

   char rpath[4097];
   realpath(argv[1], rpath);
   rpath[4096] = '\0';
   TabManager_add(tabs, rpath, basename(argv[1]), NULL);

   restoreRecentHistory(tabs, 1);

   Clipboard* clip = Clipboard_new();
   char bookmarkX[256];
   char bookmarkY[256];
   for (int i = 0; i < 256; i++) {
      bookmarkX[i] = 0;
      bookmarkY[i] = 0;
   }

   Field* gotoField = Field_new("Go to line:", 0, LINES - 1, MIN(20, COLS - 20));

   Field* findField = Field_new("Find:", 0, LINES - 1, MIN(100, COLS - 20));
   Field* replaceField = Field_new("Replace with:", 0, LINES - 1, MIN(100, COLS - 20));

   bkgdset(NormalColor);
   
   Buffer* buffer = TabManager_draw(tabs);
   Buffer_goto(buffer, 0, jump - 1);

   int ch = 0;
   while (!quit) {
      int y, x;

      attrset(CRT_colors[TabColor]);
      mvhline(LINES - 1, 0, ' ', COLS);

      Buffer* buffer = TabManager_draw(tabs);
      Script_setCurrentBuffer(buffer);
      getyx(stdscr, y, x);

      attrset(CRT_colors[TabColor]);
      mvprintw(LINES - 1, 0, "L:%d C:%d %s",
                             buffer->y + 1, buffer->x + 1,
                             (buffer->tabCharacters ? " TABS" : ""));

      attrset(A_NORMAL);
      buffer->lastKey = ch;
      move(y, x);
      ch = CRT_getCharacter();
      
      //TODO: mouse

      if (buffer->marking) {
         switch (ch) {
         case KEY_C_RIGHT: ch = KEY_CS_RIGHT; break;
         case KEY_C_LEFT:  ch = KEY_CS_LEFT;  break;
         case KEY_C_UP:    ch = KEY_CS_UP;    break;
         case KEY_C_DOWN:  ch = KEY_CS_DOWN;  break;
         case KEY_C_HOME:  ch = KEY_CS_HOME;  break;
         case KEY_C_END:   ch = KEY_CS_END;   break;
         case KEY_RIGHT:   ch = KEY_S_RIGHT;  break;
         case KEY_LEFT:    ch = KEY_S_LEFT;   break;
         case KEY_UP:      ch = KEY_S_UP;     break;
         case KEY_DOWN:    ch = KEY_S_DOWN;   break;
         case KEY_HOME:    ch = KEY_S_HOME;   break;
         case KEY_END:     ch = KEY_S_END;    break;
         default:          buffer->marking = false;
         }
      }

      switch (ch) {
      case ERR:
         continue;
      case KEY_RESIZE:
         TabManager_resize(tabs, COLS, LINES);
         findField->y = LINES - 1;
         break;
      case KEY_CTRL('Y'):
      {
         statusMessage("Press a key to go to a bookmark; Ctrl+Y: create bookmark; Esc: cancel");
         buffer->selecting = false;
         buffer->marking = false;
         int key = getch();
         if (key == KEY_CTRL('Y')) {
            statusMessage("Enter key to save bookmark into");
            do {
               key = getch();
            } while (key < 1 || key > 255);
            if (key == 27)
               break;
            bookmarkX[key] = buffer->x;
            bookmarkY[key] = buffer->y;
         } else if (key > 0 && key < 256){
            if (bookmarkX[key]) {
               Buffer_goto(buffer, bookmarkX[key], bookmarkY[key]);
            } else {
               beep();
            }
         }
         break;
      }
      case KEY_CTRL('B'):
      {
         buffer->marking = !(buffer->marking);
         break;
      }
      case KEY_C_INSERT:
      case KEY_CTRL('C'):
      {
         int blockLen;
         char* block = Buffer_copyBlock(buffer, &blockLen);
         if (block) {
            Clipboard_set(clip, block, blockLen);
         }
         buffer->selecting = false;
         break;
      }
      case KEY_CTRL('P'):
      {
         statusMessage("Pull or push with <- or ->");

         buffer->selecting = false;
         buffer->marking = false;
         int mode = getch();
         switch (mode)
         {
         case KEY_LEFT:
            Buffer_pullText(buffer);
            break;
         case KEY_RIGHT:
            Buffer_pushText(buffer);
            break;
         default:
            break;
         }
         break;
      }
      case KEY_CTRL('G'):
      {
         clearStatusBar();
         Field_start(gotoField);
         bool quit = false;
         int saveX = buffer->x;
         int saveY = buffer->y;
         int lastY = 0;
         while (!quit) {
            bool handled;
            int ch = Field_run(gotoField, false, &handled);
            if (!handled) {
               if (ch >= '0' && ch <= '9')
                  Field_insertChar(gotoField, ch);
               else if (ch == 13) {
                  quit = true; 
               } else if (ch == 27) {
                  quit = true;
                  Buffer_goto(buffer, saveX, saveY);
               }
            }
            int y = atoi(gotoField->current->text) - 1;
            if (y != lastY) {
               Buffer_goto(buffer, 0, y);
               Buffer_draw(buffer);
               lastY = y;
            }
         }
         break;
      }
      case KEY_F(3):
      case KEY_CTRL('F'):
      {
         clearStatusBar();
         Field_start(findField);
         bool quit = false;
         int saveX = buffer->x;
         int saveY = buffer->y;
         bool wrapped = false;
         int firstX = -1;
         int firstY = -1;
         bool caseSensitive = false;
         bool searched = false;
         bool found = false;
         while (!quit) {
            bool handled;
            Field_printfLabel(findField, "Lin=%d Col=%d (%c)Case %sFind:", buffer->y + 1, buffer->x + 1, caseSensitive ? '*' : ' ', wrapped ? "Wrapped " : "");
            int ch = Field_run(findField, false, &handled);
            if (!handled) {
               if (ch == 9 || (ch >= 32 && ch <= 255)) {
                  Field_insertChar(findField, ch);
                  wrapped = false;                  
                  firstX = -1;
                  firstY = -1;
                  found = Buffer_find(buffer, findField->current->text, false, caseSensitive, true);
                  searched = true;
               } else {
                  switch (ch) {
                  case KEY_C_UP:
                     Buffer_slideUpLine(buffer);
                     Buffer_draw(buffer);
                     break;
                  case KEY_C_DOWN:
                     Buffer_slideDownLine(buffer);
                     Buffer_draw(buffer);
                     break;
                  case KEY_CTRL('I'):
                  case KEY_CTRL('C'):
                  case KEY_F(2):
                  {
                     caseSensitive = !caseSensitive;
                  }
                  case KEY_F(3):
                  case KEY_CTRL('F'):
                  {
                     found = Buffer_find(buffer, findField->current->text, true, caseSensitive, true);
                     searched = true;
                     break;
                  }
                  case KEY_CTRL('P'):
                  {
                     found = Buffer_find(buffer, findField->current->text, true, caseSensitive, false);
                     searched = true;
                     break;
                  }
                  case KEY_CTRL('R'):
                  {
                     int rch = 0;
                     while (true) {
                        if (searched) {
                           if (found) {
                              if (firstX == -1) {
                                 firstX = buffer->x;
                                 firstY = buffer->y;
                              } else {
                                 if (buffer->y == firstY && buffer->x == firstX)
                                    wrapped = true;
                              }
                              findField->fieldColor = CRT_colors[FieldColor];
                              Buffer_draw(buffer);
                           } else {
                              findField->fieldColor = CRT_colors[FieldFailColor];
                              break;
                           }
                        }
                        bool quitMask[255] = {0};
                        quitMask[KEY_CTRL('R')] = true;
                        quitMask[KEY_CTRL('F')] = true;
                        quitMask[KEY_CTRL('P')] = true;
                        rch = Field_quickRun(replaceField, quitMask);
                        if (rch == 13 || rch == KEY_CTRL('R')) {
                           if (buffer->selecting) {
                              Buffer_deleteChar(buffer);
                              Buffer_pasteBlock(buffer, replaceField->current->text, strlen(replaceField->current->text));
                              buffer->selecting = false;
                              Buffer_draw(buffer);
                              found = Buffer_find(buffer, findField->current->text, true, caseSensitive, true);
                              searched = true;
                           }
                        } else if (rch == KEY_CTRL('P')) {
                           found = Buffer_find(buffer, findField->current->text, true, caseSensitive, false);
                           searched = true;
                        } else if (rch == 27) {
                           break;
                        } else {
                           found = Buffer_find(buffer, findField->current->text, true, caseSensitive, true);
                           searched = true;
                        }
                     }
                     break;             
                  }
                  default:
                     quit = true;
                     if (ch == 27)
                        Buffer_goto(buffer, saveX, saveY);
                  }
               }
            } else {
               if (ch == KEY_UP || ch == KEY_DOWN) {
                  Buffer_goto(buffer, saveX, saveY);
                  wrapped = false;
                  firstX = -1;
                  firstY = -1;
                  found = Buffer_find(buffer, findField->current->text, true, caseSensitive, true);
                  searched = true;
               }
            }
            if (searched) {
               if (found) {
                  if (firstX == -1) {
                     firstX = buffer->x;
                     firstY = buffer->y;
                  } else {
                     if (buffer->y == firstY && buffer->x == firstX)
                        wrapped = true;
                  }
                  findField->fieldColor = CRT_colors[FieldColor];
                  Buffer_draw(buffer);
               } else
                  findField->fieldColor = CRT_colors[FieldFailColor];
            }
         }
         buffer->selecting = false;
         break;
      }
      case KEY_CTRL('S'):
      {
         attemptSave(buffer);
         break;
      }
      case KEY_CTRL('L'):
      {
         attrset(NormalColor);
         clear();
         Buffer_refreshHighlight(buffer);
         break;
      }
      case KEY_CTRL('U'):
      {
         Buffer_undo(buffer);
         break;
      }
      case KEY_SIC:
      case KEY_S_INSERT:
      case KEY_CS_INSERT:
      case KEY_CTRL('V'):
      {
         int blockLen;
         char* block = Clipboard_get(clip, &blockLen);
         if (block) {
            Buffer_pasteBlock(buffer, block, blockLen);
         }
         free(block);
         buffer->selecting = false;
         break;
      }
      case KEY_CTRL('W'):
      {
         buffer->selecting = false;
         Buffer_wordWrap(buffer, 80);
         break;
      }
      case KEY_SDC:
      case KEY_S_DELETE:
      case KEY_CTRL('X'):
      {
         int blockLen;
         char* block = Buffer_copyBlock(buffer, &blockLen);
         if (block) {
            Clipboard_set(clip, block, blockLen);
            Buffer_deleteBlock(buffer);
         }
         buffer->selecting = false;
         break;
      }
      case KEY_F(10):
      case KEY_CTRL('Q'):
      {
         bool reallyQuit = true;
         int items = Vector_size(tabs->items);
         for (int i = 0; i < items; i++) {
            TabManager_setPage(tabs, i);
            TabPage* page = (TabPage*) Vector_get(tabs->items, i);
            buffer = page->buffer;
         
            if (buffer && buffer->modified) {
               TabManager_draw(tabs);
               attrset(CRT_colors[StatusColor]);
               mvprintw(LINES - 1, 0, "Buffer was modified. Save before exit? [y/n/c]");
               clrtoeol();
               attrset(CRT_colors[NormalColor]);
               refresh();
               int opt;
               do {
                  beep();
                  opt = getch();
               } while (opt != 'y' && opt != 'n' && opt != 'c');
               if (opt == 'y')
                  if (!attemptSave(buffer)) {
                     reallyQuit = false;
                     break;
                  }
               if (opt == 'c') {
                  reallyQuit = false;
                  break;
               }
            }
         }
         if (reallyQuit)
            quit = 1;
         break;
      }
      case 0x0d:
      case KEY_ENTER:
         Buffer_breakLine(buffer);
         buffer->selecting = false;
         break;
      case '\t':
      {
         Buffer_indent(buffer);
         /*
         char word[100];
         int at = Buffer_currentWord(buffer, word, 100);
         if (*word && at == strlen(word)) {
            char rest[100];
            if (PatternMatcher_partialMatch(buffer->hl->words->start, (unsigned char*) word, at, rest, 100)) {
               Buffer_pasteBlock(buffer, rest, strlen(rest));
            }
         } else {
            Buffer_indent(buffer);
         }
         */
         break;
      }
      case KEY_BTAB:
         Buffer_unindent(buffer);
         break;
      case KEY_CS_RIGHT:
         Buffer_select(buffer, Buffer_forwardWord);
         break;
      case KEY_C_RIGHT:
         Buffer_forwardWord(buffer);
         buffer->selecting = false;
         break;
      case KEY_S_RIGHT:
         Buffer_select(buffer, Buffer_forwardChar);
         break;
      case KEY_RIGHT:
         Buffer_forwardChar(buffer);
         buffer->selecting = false;
         break;
      case KEY_CS_LEFT:
         Buffer_select(buffer, Buffer_backwardWord);
         break;
      case KEY_C_LEFT:
         Buffer_backwardWord(buffer);
         buffer->selecting = false;
         break;
      case KEY_S_LEFT:
         Buffer_select(buffer, Buffer_backwardChar);
         break;
      case KEY_LEFT:
         Buffer_backwardChar(buffer);
         buffer->selecting = false;
         break;
      case KEY_CS_DOWN:
         Buffer_select(buffer, Buffer_slideDownLine);
         break;
      case KEY_C_DOWN:
         Buffer_slideDownLine(buffer);
         buffer->selecting = false;
         break;
      case KEY_S_DOWN:
         Buffer_select(buffer, Buffer_downLine);
         break;
      case KEY_DOWN:
         Buffer_downLine(buffer);
         buffer->selecting = false;
         break;
      case KEY_CS_UP:
         Buffer_select(buffer, Buffer_slideUpLine);
         break;
      case KEY_C_UP:
         Buffer_slideUpLine(buffer);
         buffer->selecting = false;
         break;
      case KEY_S_UP:
         Buffer_select(buffer, Buffer_upLine);
         break;
      case KEY_UP:
         Buffer_upLine(buffer);
         buffer->selecting = false;
         break;
      case KEY_CS_HOME:
      case KEY_S_HOME:
         Buffer_select(buffer, Buffer_beginningOfLine);
         break;
      case KEY_CS_END:
      case KEY_S_END:
         Buffer_select(buffer, Buffer_endOfLine);
         break;
      case KEY_CS_PPAGE:
      case KEY_S_PPAGE:
         Buffer_select(buffer, Buffer_previousPage);
         break;
      case KEY_CS_NPAGE:
      case KEY_S_NPAGE:
         Buffer_select(buffer, Buffer_nextPage);
         break;
      case KEY_CTRL('A'):
      case KEY_HOME:
         Buffer_beginningOfLine(buffer);
         buffer->selecting = false;
         break;
      case KEY_CTRL('E'):
      case KEY_END:
         Buffer_endOfLine(buffer);
         buffer->selecting = false;
         break;
      case KEY_C_HOME:
         Buffer_beginningOfFile(buffer);
         buffer->selecting = false;
         break;
      case KEY_C_END:
         Buffer_endOfFile(buffer);
         buffer->selecting = false;
         break;
      case KEY_DC:
         Buffer_deleteChar(buffer);
         buffer->selecting = false;
         break;
      case '\177':
      case KEY_BACKSPACE:
         Buffer_backwardDeleteChar(buffer);
         buffer->selecting = false;
         break;
      case KEY_CTRL('T'):
         buffer->tabCharacters = !buffer->tabCharacters;
         break;
      case KEY_CTRL('J'):
         TabManager_previousPage(tabs);
         ch = 0;
         // Buffer_defaultKeyHandler(buffer, KEY_PPAGE);
         break;
      case KEY_CTRL('K'):
         TabManager_nextPage(tabs);
         ch = 0;
         // Buffer_defaultKeyHandler(buffer, KEY_NPAGE);
         break;
      default:
         Buffer_defaultKeyHandler(buffer, ch);
         break;
      }
   }

   attron(CRT_colors[NormalColor]);
   mvhline(LINES-1, 0, ' ', COLS);
   attroff(CRT_colors[NormalColor]);
   refresh();
   
   CRT_done();
   Clipboard_delete(clip);
   Field_delete(gotoField);
   Field_delete(findField);
   Field_delete(replaceField);
   storeRecentHistory(tabs);
   TabManager_delete(tabs);
   debug_done();
   return 0;
}

/** This is a very long line. 1 This is a very long line. 2 This is a very long line. 3 This is a very long line. 4 This is a very long line. 5 This is a very long line. */
