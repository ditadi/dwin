#import "mac_status_bar.h"

@implementation MacStatusBar

- (void)setup {
  // create status bar item
  self.statusItem = [[NSStatusBar systemStatusBar]
      statusItemWithLength:NSVariableStatusItemLength];

  // set icon (sf symbols on macOS 11+, fallback to text)
  if (@available(macOS 11.0, *)) {
    NSImage *icon = [NSImage imageWithSystemSymbolName:@"square.stack.3d.up"
                              accessibilityDescription:@"dwin"];
    icon.template = YES;
    self.statusItem.button.image = icon;
  } else {
    self.statusItem.button.title = @"dwin";
  }

  [self buildMenu];
}

- (void)buildMenu {
  self.menu = [[NSMenu alloc] init];

  // header
  NSMenuItem *header = [[NSMenuItem alloc] initWithTitle:@"dwin v0.1.0"
                                                  action:nil
                                           keyEquivalent:@""];
  header.enabled = NO;
  [self.menu addItem:header];
  [self.menu addItem:[NSMenuItem separatorItem]];

  // quit
  NSMenuItem *quit = [[NSMenuItem alloc] initWithTitle:@"Quit"
                                                action:@selector(terminate:)
                                         keyEquivalent:@"q"];
  quit.target = self;
  [self.menu addItem:quit];

  self.statusItem.menu = self.menu;
}

- (void)refresh {
  // @TODO: implement
}

- (void)terminate:(id)sender {
  (void)sender;
  NSLog(@"[dwin] Quit requested...");
  [NSApp terminate:nil];
}

@end
