# wow-tofuworld-bot-spells

Tofu Worldview module (installs as `mod-tofu-bot-spells`) that lets mod-playerbots bots cast custom spells.

Playerbots looks spells up by name among the spells a bot knows, but many of its strategies only switch a spell on when the bot knows a stock spell ID. This module adds one combat strategy, `tofu bot spells`, whose lines come from `TofuBotSpells.Rules` in the conf: each rule is `spellId trigger>action!priority`, and a rule is added to a bot only if the bot knows that spell ID.

No playerbots file is edited. On the first world tick the strategy is registered into the ten class strategy registries of mod-playerbots and `+tofu bot spells` is appended to playerbots' `CombatStrategies` and `RandomBotCombatStrategies`, so it survives strategy resets.

## Install

1. Place the repo at `modules/mod-tofu-bot-spells` (the folder name sets the loader function `Addmod_tofu_bot_spellsScripts`).
2. Re-run CMake and build.
3. Copy `conf/mod_tofu_bot_spells.conf.dist` to the server's `configs/modules` as `mod_tofu_bot_spells.conf`.

Startup log line when it works: `mod-tofu-bot-spells: registered strategy 'tofu bot spells' with N rule(s) into all class registries of mod-playerbots.`

## Limits

- Rules and the enable switch are read once at startup.
- A bot's lines are chosen when its combat engine is built (login, group change, talent change, `reset`), so a bot that learns the spell later picks it up at its next reset or login.
- Only existing playerbots trigger and action names can be used; a custom spell with a brand-new name needs a new playerbots action.
- Requires mod-playerbots in the same build.
