/*
 * mod-tofu-bot-spells (Tofu Worldview): lets mod-playerbots bots cast custom spells.
 *
 * Playerbots finds a spell by name among the spells a bot knows, but most of its strategies switch a spell on only when the bot knows one of the stock spell IDs (RacialsStrategy checks 28730, 25046, and 50613 for Arcane Torrent). A custom spell with a stock name is therefore learned but never cast.
 *
 * This module adds one combat strategy, "tofu bot spells", built from TofuBotSpells.Rules: each rule is "spellId trigger>action!priority". When a bot's combat engine is built, the strategy adds a rule's trigger line only if the bot knows that spell ID, so a rule never affects a bot without the custom spell.
 *
 * No playerbots file is edited. The strategy is registered into the ten class strategy registries (public statics of the class AiObjectContexts) on the first world tick, before any bot logs in, and "+tofu bot spells" is appended to playerbots' CombatStrategies and RandomBotCombatStrategies so it survives every strategy reset. mod-dungeon-clear registers its strategies the same way.
 */

#include "Config.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"

#include "Action.h"
#include "NamedObjectContext.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Strategy.h"
#include "Trigger.h"

#include "DKAiObjectContext.h"
#include "DruidAiObjectContext.h"
#include "HunterAiObjectContext.h"
#include "MageAiObjectContext.h"
#include "PaladinAiObjectContext.h"
#include "PriestAiObjectContext.h"
#include "RogueAiObjectContext.h"
#include "ShamanAiObjectContext.h"
#include "WarlockAiObjectContext.h"
#include "WarriorAiObjectContext.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::string const STRATEGY_NAME = "tofu bot spells";

    struct BotSpellRule
    {
        uint32 spellId;
        std::string trigger;
        std::vector<NextAction> actions;
    };

    // Filled once on the first world tick and read-only afterwards (bots build their engines on map threads).
    std::vector<BotSpellRule> rules;

    std::string Trim(std::string const& text)
    {
        size_t const first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return "";
        size_t const last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    std::vector<std::string> Split(std::string const& text, char separator)
    {
        std::vector<std::string> parts;
        std::stringstream stream(text);
        std::string part;
        while (std::getline(stream, part, separator))
            parts.push_back(part);
        return parts;
    }

    // Rules are separated by ';'. A rule is "spellId trigger name>action name!priority", with several actions allowed after '>' separated by ','. The priority defaults to 15 (playerbots' racials use ACTION_NORMAL + 5).
    std::vector<BotSpellRule> ParseRules(std::string const& configString)
    {
        std::vector<BotSpellRule> parsed;
        for (std::string const& rawRule : Split(configString, ';'))
        {
            std::string const ruleText = Trim(rawRule);
            if (ruleText.empty())
                continue;

            size_t const space = ruleText.find(' ');
            size_t const arrow = ruleText.find('>');
            if (space == std::string::npos || arrow == std::string::npos || arrow < space)
            {
                LOG_ERROR("module", "mod-tofu-bot-spells: cannot read rule '{}'. Expected 'spellId trigger>action!priority'.", ruleText);
                continue;
            }

            BotSpellRule rule;
            rule.spellId = uint32(std::strtoul(ruleText.substr(0, space).c_str(), nullptr, 10));
            rule.trigger = Trim(ruleText.substr(space + 1, arrow - space - 1));
            for (std::string const& rawAction : Split(ruleText.substr(arrow + 1), ','))
            {
                std::string const actionText = Trim(rawAction);
                if (actionText.empty())
                    continue;
                size_t const bang = actionText.find('!');
                std::string const name = Trim(actionText.substr(0, bang));
                float const priority = bang == std::string::npos ? 15.0f : float(std::atof(actionText.substr(bang + 1).c_str()));
                if (!name.empty())
                    rule.actions.emplace_back(name, priority);
            }

            if (!rule.spellId || rule.trigger.empty() || rule.actions.empty())
            {
                LOG_ERROR("module", "mod-tofu-bot-spells: rule '{}' has no spell ID, trigger, or action; skipped.", ruleText);
                continue;
            }
            parsed.push_back(rule);
        }
        return parsed;
    }

    void AppendStrategy(std::string& strategyList)
    {
        if (strategyList.find("+" + STRATEGY_NAME) != std::string::npos)
            return;
        if (!Trim(strategyList).empty())
            strategyList += ",";
        strategyList += "+" + STRATEGY_NAME;
    }

    template <class Ctx>
    void ValidateRuleNames()
    {
        for (BotSpellRule const& rule : rules)
        {
            if (!Ctx::sharedTriggerContexts.creators.count(rule.trigger))
                LOG_ERROR("module", "mod-tofu-bot-spells: unknown playerbots trigger '{}' (spell {}). The rule will do nothing.", rule.trigger, rule.spellId);
            for (NextAction action : rule.actions)
                if (!Ctx::sharedActionContexts.creators.count(action.getName()))
                    LOG_ERROR("module", "mod-tofu-bot-spells: unknown playerbots action '{}' (spell {}). The rule will do nothing.", action.getName(), rule.spellId);
        }
    }
}

class TofuBotSpellsStrategy : public Strategy
{
public:
    TofuBotSpellsStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return STRATEGY_NAME; }

    void InitTriggers(std::vector<TriggerNode*>& triggers) override
    {
        Player* bot = botAI ? botAI->GetBot() : nullptr;
        if (!bot)
            return;
        for (BotSpellRule const& rule : rules)
            if (bot->HasSpell(rule.spellId))
                triggers.push_back(new TriggerNode(rule.trigger, rule.actions));
    }
};

class TofuBotSpellsStrategyContext : public NamedObjectContext<Strategy>
{
public:
    TofuBotSpellsStrategyContext()
    {
        creators[STRATEGY_NAME] = [](PlayerbotAI* botAI) -> Strategy* { return new TofuBotSpellsStrategy(botAI); };
    }
};

class TofuBotSpells_WorldScript : public WorldScript
{
public:
    TofuBotSpells_WorldScript() : WorldScript("TofuBotSpells_WorldScript") {}

    // The first world tick: the config is loaded, playerbots has built its registries, and no bot has logged in yet.
    void OnUpdate(uint32 /*diff*/) override
    {
        if (registered)
            return;
        registered = true;

        if (!sConfigMgr->GetOption<bool>("TofuBotSpells.Enable", true))
        {
            LOG_INFO("module", "mod-tofu-bot-spells: disabled by TofuBotSpells.Enable.");
            return;
        }

        rules = ParseRules(sConfigMgr->GetOption<std::string>("TofuBotSpells.Rules", ""));
        if (rules.empty())
        {
            LOG_INFO("module", "mod-tofu-bot-spells: no rules in TofuBotSpells.Rules; nothing registered.");
            return;
        }

        RegisterIn<WarriorAiObjectContext>();
        RegisterIn<PaladinAiObjectContext>();
        RegisterIn<DruidAiObjectContext>();
        RegisterIn<DKAiObjectContext>();
        RegisterIn<HunterAiObjectContext>();
        RegisterIn<MageAiObjectContext>();
        RegisterIn<PriestAiObjectContext>();
        RegisterIn<RogueAiObjectContext>();
        RegisterIn<ShamanAiObjectContext>();
        RegisterIn<WarlockAiObjectContext>();
        ValidateRuleNames<WarriorAiObjectContext>();

        AppendStrategy(sPlayerbotAIConfig.CombatStrategies);
        AppendStrategy(sPlayerbotAIConfig.RandomBotCombatStrategies);

        LOG_INFO("module", "mod-tofu-bot-spells: registered strategy '{}' with {} rule(s) into all class registries of mod-playerbots.", STRATEGY_NAME, rules.size());
    }

private:
    template <class Ctx>
    static void RegisterIn()
    {
        Ctx::sharedStrategyContexts.Add(new TofuBotSpellsStrategyContext());
    }

    bool registered = false;
};

void AddSC_tofu_bot_spells()
{
    new TofuBotSpells_WorldScript();
}
