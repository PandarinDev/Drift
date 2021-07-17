## Drift
### A light-weight, stylish and modern X11 stacking window manager.
Drift was born after the observation that most Linux stacking window managers are lacking in either user experience or aesthetics.
It aims to bring sensible defaults, modern decorations and animations out of the box, while remaining light-weight and configurable.

Drift sets out to achieve the following:
* Light-weight and Fast
  * Problem: Window managers are central to the desktop user experience. They should be small, fast and responsive.
  * Solution: Drift is written in modern C++ and uses XCB for asynchronous event handling.
* Portable
  * Problem: Often window managers are written with a certain desktop environment in mind.
  * Solution: Drift can be used standalone or with any desktop environment of your choice that supports custom WMs.
* Unobtrusive
  * Problem: Many window managers overwrite system configuration files (eg. fontconfig, theming of GTK/Qt applications, etc.).
  * Solution: Drift respect your configurations. It never touches any files outside of it's own configuration.
* Sensible Defaults Out of the Box
  * Problem: You shouldn't have to spend hours configuring your window manager to have sensible key bindings for basic features.
  * Solution: Drift comes with sensible, well known default shortcuts for basic features (window snapping, opening terminals, etc.).
* Just Enough Features
  * Problem: Window managers are often bloated with gimmicky features but fall short in the list of useful features.
  * Solution: Drift strives to implement just the most essential features: Quadratic window snapping, always-on-top, show desktop and a few animations. That's it.
* Modern Look and Feel
  * Problem: Most window managers look like they're from the Windows XP days, unless you download custom themes for them.
  * Solution: Drift uses modern design principles such as material design to look great from the first start.
* Easy Configuration
  * Problem: When you do want to change something, you shouldn't have to recompile your WM, write LUA/shell scripts or hack on undocumented XML files.
  * Solution: Drift comes with a GUI configuration tool and uses SQLite under the hood that you can directly write if you want to configure things manually.

# FAQ:
* Q: What stage is Drift currently in? Is it ready for use?
* A: It is purely theoretical with a proof-of-concept in the works. No, it's not even close to ready as of now.
* Q: Why bundle a GUI configuration tool with a light-weight WM?
* A: Because this way we can ensure that the version of the configuration tool and the WM (and thus the possible configuration keys/values) are always matching.
* Q: What's wrong with Openbox/Fluxbox/KWin/*[insert stacking WM of your choice here]*?
* A: Nothing, I just think we can do even better than existing solutions. In fact the project is heavily inspired by the feature set of KWin and the unobtrusiveness and light-weightness of Openbox.