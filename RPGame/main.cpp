#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <memory>
#include <cmath>
#include <ctime>
#include <deque>
#include <iostream>
#include <array>

static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));

int roll_int(int a, int b)
{
    std::uniform_int_distribution<int> d(a, b);
    return d(rng);
}

template<typename T>
void shuffle_vec(std::vector<T>& v)
{
    std::shuffle(v.begin(), v.end(), rng);
}

enum class ElementType { Fire = 0, Water = 1, Earth = 2, Nature = 3, None = 4 };
enum class AbilityKind { Damage, Heal, Shield };
enum class MethodKind { Single, Area, ChainLeft, ChainRight };
enum class EnemyIntentKind { Attack, Shield, Heal };

struct LevelConfig
{
    int enemyCount = 1;
    int hpBase = 20;
    int attackBase = 7;
    int shieldBase = 6;
    int healBase = 4;
};

struct EnemyIntent
{
    EnemyIntentKind kind = EnemyIntentKind::Attack;
    ElementType element = ElementType::None;
    int value = 0;
    std::string label;
};

struct Effect
{
    int value = 0;
    AbilityKind kind = AbilityKind::Damage;
    ElementType element = ElementType::None;
};

struct TimedStatus
{
    AbilityKind kind = AbilityKind::Heal;
    ElementType element = ElementType::None;
    MethodKind method = MethodKind::Single;
    int baseValue = 0;
    int age = 0;
    int ticksLeft = 0;
};

struct Unit;
struct World;

struct Element
{
    virtual ~Element() = default;
    virtual int modifyAmount(int base, Unit& target) const = 0;
    virtual void onHit(Unit& target) const {}
    virtual ElementType type() const = 0;
};

struct ApplicationMethod
{
    virtual ~ApplicationMethod() = default;
    virtual void apply(World& world, Unit& caster, std::vector<int> targets, const Effect& eff) const = 0;
    virtual MethodKind methodKind() const = 0;
};

struct Unit
{
    std::string name;
    int hp = 0;
    int maxHp = 0;
    int armor = 0;
    ElementType elem = ElementType::None;
    ElementType shieldElement = ElementType::None;

    bool alive() const { return hp > 0; }

    void heal(int val)
    {
        hp = std::min(maxHp, hp + val);
    }
};

std::string elementToString(ElementType e)
{
    switch (e)
    {
    case ElementType::Fire: return "Fire";
    case ElementType::Water: return "Water";
    case ElementType::Earth: return "Earth";
    case ElementType::Nature: return "Nature";
    default: return "None";
    }
}

std::string elementToStringLower(ElementType e)
{
    switch (e)
    {
    case ElementType::Fire: return "fire";
    case ElementType::Water: return "water";
    case ElementType::Earth: return "earth";
    case ElementType::Nature: return "nature";
    default: return "none";
    }
}

std::string intentToString(EnemyIntentKind k)
{
    switch (k)
    {
    case EnemyIntentKind::Attack: return "ATK";
    case EnemyIntentKind::Shield: return "SHIELD";
    case EnemyIntentKind::Heal: return "HEAL";
    }
    return "";
}

std::string methodToStringLower(MethodKind m)
{
    switch (m)
    {
    case MethodKind::Single: return "single target";
    case MethodKind::Area: return "aoe";
    case MethodKind::ChainLeft: return "chain left";
    case MethodKind::ChainRight: return "chain right";
    }
    return "";
}

std::string kindToStringLower(AbilityKind k)
{
    switch (k)
    {
    case AbilityKind::Damage: return "damage";
    case AbilityKind::Heal: return "heal";
    case AbilityKind::Shield: return "shield";
    }
    return "";
}

std::string buildAbilityDescription(AbilityKind kind, ElementType elem, MethodKind method)
{
    if (kind == AbilityKind::Damage)
        return methodToStringLower(method) + " " + elementToStringLower(elem) + " damage";

    if (kind == AbilityKind::Heal)
    {
        if (method == MethodKind::Single)
            return "single target " + elementToStringLower(elem) + " heal x2";
        if (method == MethodKind::Area)
            return "aoe " + elementToStringLower(elem) + " regen 3 turns";
        if (method == MethodKind::ChainLeft)
            return "chain left " + elementToStringLower(elem) + " heal over 3 turns";
        if (method == MethodKind::ChainRight)
            return "chain right " + elementToStringLower(elem) + " heal over 3 turns";
    }

    if (kind == AbilityKind::Shield)
    {
        if (method == MethodKind::Single)
            return "single target " + elementToStringLower(elem) + " shield x2";
        if (method == MethodKind::Area)
            return "aoe " + elementToStringLower(elem) + " shield over 3 turns";
        if (method == MethodKind::ChainLeft)
            return "chain left " + elementToStringLower(elem) + " shield over 3 turns";
        if (method == MethodKind::ChainRight)
            return "chain right " + elementToStringLower(elem) + " shield over 3 turns";
    }

    return methodToStringLower(method) + " " + elementToStringLower(elem) + " " + kindToStringLower(kind);
}

float effectiveness(ElementType attacker, ElementType defender)
{
    if (attacker == ElementType::Fire && defender == ElementType::Nature) return 2.f;
    if (attacker == ElementType::Nature && defender == ElementType::Earth) return 2.f;
    if (attacker == ElementType::Earth && defender == ElementType::Water) return 2.f;
    if (attacker == ElementType::Water && defender == ElementType::Fire) return 2.f;

    if (defender == ElementType::Fire && attacker == ElementType::Nature) return 0.5f;
    if (defender == ElementType::Nature && attacker == ElementType::Earth) return 0.5f;
    if (defender == ElementType::Earth && attacker == ElementType::Water) return 0.5f;
    if (defender == ElementType::Water && attacker == ElementType::Fire) return 0.5f;

    return 1.f;
}

void clearTempShield(Unit& u)
{
    u.armor = 0;
    u.shieldElement = ElementType::None;
}

float healMultiplier(ElementType healElement, ElementType lastDamageElement)
{
    if (lastDamageElement == ElementType::None) return 1.f;
    if (effectiveness(healElement, lastDamageElement) == 2.f) return 1.5f;
    return 1.f;
}

float healMultiplierFromAttacks(ElementType healElement, const std::vector<ElementType>& attacks)
{
    for (auto a : attacks)
    {
        if (effectiveness(healElement, a) == 2.f)
            return 1.5f;
    }
    return 1.f;
}

void applyDamage(Unit& target, int baseDamage, ElementType attackElement)
{
    if (!target.alive()) return;

    float mult = effectiveness(attackElement, target.elem);
    int dmg = static_cast<int>(std::round(baseDamage * mult));

    int effectiveArmor = target.armor;
    if (target.armor > 0 && target.shieldElement != ElementType::None &&
        effectiveness(target.shieldElement, attackElement) == 2.0f)
    {
        effectiveArmor = target.armor * 2;
    }

    int dmgLeft = std::max(0, dmg - effectiveArmor);
    target.armor = std::max(0, target.armor - dmg);
    target.hp = std::max(0, target.hp - dmgLeft);
}

struct ElementalEnemy : Unit
{
    int levelIndex = 1;
    int tier = 1;
    int attackValue = 0;
    int shieldValue = 0;
    int healValue = 0;
    EnemyIntent intent;

    static ElementalEnemy create(ElementType et, int levelIndex, const LevelConfig& cfg)
    {
        ElementalEnemy e;
        e.elem = et;
        e.levelIndex = levelIndex;

        int maxTier = 1 + levelIndex / 2;
        if (maxTier > 3) maxTier = 3;
        e.tier = roll_int(1, maxTier);

        e.maxHp = cfg.hpBase + (levelIndex - 1) * 4 + (e.tier - 1) * 6;
        e.hp = e.maxHp;

        e.attackValue = cfg.attackBase + (levelIndex - 1) + (e.tier - 1) * 2;
        e.shieldValue = cfg.shieldBase + (levelIndex - 1) + (e.tier - 1) * 2;
        e.healValue = cfg.healBase + (levelIndex - 1) / 2 + (e.tier - 1);

        e.name = elementToString(et) + " Elemental";
        e.armor = 0;
        e.shieldElement = ElementType::None;
        return e;
    }

    void rollIntent()
    {
        int attackWeight = 52 + (levelIndex - 1) * 4;
        int shieldWeight = 28;
        int healWeight = 20 - (levelIndex - 1) * 2;
        if (healWeight < 10) healWeight = 10;

        int total = attackWeight + shieldWeight + healWeight;
        int r = roll_int(1, total);

        if (r <= attackWeight)
        {
            intent.kind = EnemyIntentKind::Attack;
            intent.element = elem;
            intent.value = attackValue;
            intent.label = intentToString(EnemyIntentKind::Attack) + " " + std::to_string(attackValue) + " [" + elementToString(elem) + "]";
        }
        else if (r <= attackWeight + shieldWeight)
        {
            intent.kind = EnemyIntentKind::Shield;
            intent.element = elem;
            intent.value = shieldValue;
            intent.label = intentToString(EnemyIntentKind::Shield) + " " + std::to_string(shieldValue) + " [" + elementToString(elem) + "]";
        }
        else
        {
            if (hp >= maxHp)
            {
                intent.kind = EnemyIntentKind::Attack;
                intent.element = elem;
                intent.value = attackValue;
                intent.label = intentToString(EnemyIntentKind::Attack) + " " + std::to_string(attackValue) + " [" + elementToString(elem) + "]";
            }
            else
            {
                intent.kind = EnemyIntentKind::Heal;
                intent.element = elem;
                intent.value = healValue;
                intent.label = intentToString(EnemyIntentKind::Heal) + " " + std::to_string(healValue) + " [" + elementToString(elem) + "]";
            }
        }
    }
};

struct World
{
    Unit player;
    std::vector<ElementalEnemy> enemies;
    ElementType lastEnemyAttackElement = ElementType::None;
    ElementType lastPlayerAttackElement = ElementType::None;
    std::vector<ElementType> lastEnemyAttackElements;
    std::vector<TimedStatus> playerStatuses;
};

struct FireElement : Element
{
    ElementType type() const override { return ElementType::Fire; }
    int modifyAmount(int base, Unit&) const override { return base; }
};

struct WaterElement : Element
{
    ElementType type() const override { return ElementType::Water; }
    int modifyAmount(int base, Unit&) const override { return base; }
};

struct EarthElement : Element
{
    ElementType type() const override { return ElementType::Earth; }
    int modifyAmount(int base, Unit&) const override { return base; }
};

struct NatureElement : Element
{
    ElementType type() const override { return ElementType::Nature; }
    int modifyAmount(int base, Unit&) const override { return base; }
};

float timedMultiplier(MethodKind m, int age)
{
    if (m == MethodKind::Area)
        return 1.f;

    if (m == MethodKind::ChainLeft)
    {
        if (age == 0) return 0.5f;
        if (age == 1) return 1.f;
        return 2.f;
    }

    if (m == MethodKind::ChainRight)
    {
        if (age == 0) return 2.f;
        if (age == 1) return 1.f;
        return 0.5f;
    }

    return 1.f;
}

struct SingleMethod : ApplicationMethod
{
    MethodKind methodKind() const override { return MethodKind::Single; }

    void apply(World& world, Unit&, std::vector<int> targets, const Effect& eff) const override
    {
        if (eff.kind == AbilityKind::Damage)
        {
            if (targets.empty()) return;
            int idx = targets[0];
            if (idx < 0 || idx >= static_cast<int>(world.enemies.size())) return;
            applyDamage(world.enemies[idx], eff.value, eff.element);
        }
        else if (eff.kind == AbilityKind::Heal)
        {
            float mult = 2.f * healMultiplierFromAttacks(eff.element, world.lastEnemyAttackElements);
            world.player.heal(static_cast<int>(std::round(eff.value * mult)));
        }
        else if (eff.kind == AbilityKind::Shield)
        {
            world.player.armor += static_cast<int>(std::round(eff.value * 2.f));
            world.player.shieldElement = eff.element;
        }
    }
};

struct AreaMethod : ApplicationMethod
{
    MethodKind methodKind() const override { return MethodKind::Area; }

    void apply(World& world, Unit&, std::vector<int>, const Effect& eff) const override
    {
        if (eff.kind == AbilityKind::Damage)
        {
            for (auto& target : world.enemies)
                applyDamage(target, eff.value, eff.element);
        }
        else if (eff.kind == AbilityKind::Heal)
        {
            TimedStatus st;
            st.kind = AbilityKind::Heal;
            st.element = eff.element;
            st.method = MethodKind::Area;
            st.baseValue = eff.value;
            st.age = 0;
            st.ticksLeft = 3;
            world.playerStatuses.push_back(st);
        }
        else if (eff.kind == AbilityKind::Shield)
        {
            TimedStatus st;
            st.kind = AbilityKind::Shield;
            st.element = eff.element;
            st.method = MethodKind::Area;
            st.baseValue = eff.value;
            st.age = 0;
            st.ticksLeft = 3;
            world.playerStatuses.push_back(st);
        }
    }
};

struct ChainMethodLeft : ApplicationMethod
{
    MethodKind methodKind() const override { return MethodKind::ChainLeft; }

    void apply(World& world, Unit&, std::vector<int> targets, const Effect& eff) const override
    {
        if (eff.kind == AbilityKind::Damage)
        {
            if (targets.empty()) return;
            int idx = targets[0];
            if (idx < 0 || idx >= static_cast<int>(world.enemies.size())) return;

            float curMult = 1.5f;
            for (int i = idx; i >= 0 && curMult > 0.01f; --i)
            {
                if (!world.enemies[i].alive()) continue;
                int base = static_cast<int>(std::round(eff.value * curMult));
                applyDamage(world.enemies[i], base, eff.element);
                curMult *= 0.5f;
            }
        }
        else if (eff.kind == AbilityKind::Heal || eff.kind == AbilityKind::Shield)
        {
            TimedStatus st;
            st.kind = eff.kind;
            st.element = eff.element;
            st.method = MethodKind::ChainLeft;
            st.baseValue = eff.value;
            st.age = 0;
            st.ticksLeft = 3;
            world.playerStatuses.push_back(st);
        }
    }
};

struct ChainMethodRight : ApplicationMethod
{
    MethodKind methodKind() const override { return MethodKind::ChainRight; }

    void apply(World& world, Unit&, std::vector<int> targets, const Effect& eff) const override
    {
        if (eff.kind == AbilityKind::Damage)
        {
            if (targets.empty()) return;
            int idx = targets[0];
            if (idx < 0 || idx >= static_cast<int>(world.enemies.size())) return;

            float curMult = 1.5f;
            for (int i = idx; i < static_cast<int>(world.enemies.size()) && curMult > 0.01f; ++i)
            {
                if (!world.enemies[i].alive()) continue;
                int base = static_cast<int>(std::round(eff.value * curMult));
                applyDamage(world.enemies[i], base, eff.element);
                curMult *= 0.5f;
            }
        }
        else if (eff.kind == AbilityKind::Heal || eff.kind == AbilityKind::Shield)
        {
            TimedStatus st;
            st.kind = eff.kind;
            st.element = eff.element;
            st.method = MethodKind::ChainRight;
            st.baseValue = eff.value;
            st.age = 0;
            st.ticksLeft = 3;
            world.playerStatuses.push_back(st);
        }
    }
};

struct Ability
{
    std::string name;
    std::string customDescription;
    int baseValue = 0;
    AbilityKind kind = AbilityKind::Damage;
    std::shared_ptr<Element> element;
    std::shared_ptr<ApplicationMethod> method;

    virtual ~Ability() = default;

    virtual std::string description() const
    {
        if (!customDescription.empty())
            return customDescription;

        return buildAbilityDescription(kind, element ? element->type() : ElementType::None,
            method ? method->methodKind() : MethodKind::Single);
    }

    virtual void use(World& world, std::vector<int> targets)
    {
        Effect eff;
        eff.value = baseValue;
        eff.kind = kind;
        eff.element = element ? element->type() : ElementType::None;
        method->apply(world, world.player, targets, eff);
    }
};

struct SimpleAbility : Ability
{
    SimpleAbility(const std::string& n, int base, AbilityKind k, std::shared_ptr<Element> e, std::shared_ptr<ApplicationMethod> m, std::string desc = "")
    {
        name = n;
        baseValue = base;
        kind = k;
        element = std::move(e);
        method = std::move(m);
        customDescription = std::move(desc);
    }
};

struct FireAOEDamage : Ability
{
    FireAOEDamage(std::shared_ptr<Element> e, std::shared_ptr<ApplicationMethod> m)
    {
        name = "Fireball";
        baseValue = 10;
        kind = AbilityKind::Damage;
        element = std::move(e);
        method = std::move(m);
        customDescription = "aoe fire damage";
    }

    void use(World& world, std::vector<int>) override
    {
        for (auto& target : world.enemies)
            applyDamage(target, baseValue, ElementType::Fire);
    }
};

struct Card
{
    std::shared_ptr<Ability> ability;
};

struct Deck
{
    std::deque<Card> draw;
    std::vector<Card> discard;

    void shuffleIntoDraw()
    {
        for (auto& c : discard)
            draw.push_back(c);
        discard.clear();

        std::vector<Card> temp(draw.begin(), draw.end());
        shuffle_vec(temp);

        draw.clear();
        for (auto& c : temp)
            draw.push_back(c);
    }

    Card drawOne()
    {
        if (draw.empty())
            shuffleIntoDraw();

        if (draw.empty())
            return Card{ nullptr };

        Card c = draw.front();
        draw.pop_front();
        return c;
    }

    void setShuffled(const std::vector<Card>& cards)
    {
        draw.clear();
        discard.clear();
        std::vector<Card> temp = cards;
        shuffle_vec(temp);
        for (auto& c : temp)
            draw.push_back(c);
    }
};

struct Button
{
    sf::RectangleShape rect;
    sf::Text text;

    bool contains(sf::Vector2f p) const
    {
        return rect.getGlobalBounds().contains(p);
    }
};

struct CardWidget
{
    sf::RectangleShape rect;
    sf::Text name;
    sf::Text desc;
    Card card;
};

struct Game
{
    sf::RenderWindow window;
    sf::Font font;

    sf::Texture heroTexture;
    bool heroTextureLoaded = false;

    std::array<sf::Texture, 4> enemyTextures{};
    std::array<bool, 4> enemyTextureLoaded{ false, false, false, false };

    World world;
    Deck deck;
    std::vector<Card> hand;
    std::vector<Card> ownedCards;

    int playCountThisTurn = 0;
    int maxPlaysPerTurn = 3;
    int playerMaxHp = 100;
    int currentLevel = 1;

    bool inReward = false;
    std::vector<std::shared_ptr<Ability>> rewardPool;
    std::vector<std::shared_ptr<Ability>> rewardChoices;

    bool showWin = false;
    bool showLose = false;

    Button btnEndTurn;
    Button btnExit;
    std::vector<CardWidget> cardWidgets;

    std::shared_ptr<Ability> selectedAbilityForPlay = nullptr;
    int selectedTargetIndex = -1;

    std::shared_ptr<ApplicationMethod> singleMethod = std::make_shared<SingleMethod>();
    std::shared_ptr<ApplicationMethod> areaMethod = std::make_shared<AreaMethod>();
    std::shared_ptr<ApplicationMethod> chainLeftMethod = std::make_shared<ChainMethodLeft>();
    std::shared_ptr<ApplicationMethod> chainRightMethod = std::make_shared<ChainMethodRight>();

    std::shared_ptr<Element> fireElem = std::make_shared<FireElement>();
    std::shared_ptr<Element> waterElem = std::make_shared<WaterElement>();
    std::shared_ptr<Element> earthElem = std::make_shared<EarthElement>();
    std::shared_ptr<Element> natureElem = std::make_shared<NatureElement>();

    std::vector<LevelConfig> levelConfigs;

    Game()
        : window(sf::VideoMode(1280, 720), "Elemental Arena")
    {
        window.setFramerateLimit(60);

        if (!font.loadFromFile("font.ttf"))
            std::cout << "Font load failed\n";

        loadAssets();
        setupUI();
        setupLevelConfigs();
        setupPlayerAndDeck();
        startLevel(1);
    }

    void loadAssets()
    {
        heroTextureLoaded = heroTexture.loadFromFile("assets/sprites/hero.png");

        enemyTextureLoaded[0] = enemyTextures[0].loadFromFile("assets/sprites/fireElem.png");
        enemyTextureLoaded[1] = enemyTextures[1].loadFromFile("assets/sprites/waterElem.png");
        enemyTextureLoaded[2] = enemyTextures[2].loadFromFile("assets/sprites/earthElem.png");
        enemyTextureLoaded[3] = enemyTextures[3].loadFromFile("assets/sprites/natureElem.png");
    }

    void setupLevelConfigs()
    {
        levelConfigs = {
            LevelConfig{1, 34, 10, 8, 5},
            LevelConfig{2, 24, 8, 6, 4},
            LevelConfig{3, 22, 9, 7, 4},
            LevelConfig{4, 18, 10, 8, 5},
            LevelConfig{5, 16, 11, 9, 5}
        };
    }

    void setupUI()
    {
        btnEndTurn.rect.setSize({ 140.f, 40.f });
        btnEndTurn.rect.setPosition(1080.f, 665.f);
        btnEndTurn.rect.setFillColor(sf::Color(180, 180, 180));
        btnEndTurn.text.setFont(font);
        btnEndTurn.text.setCharacterSize(16);
        btnEndTurn.text.setString("End Turn");
        btnEndTurn.text.setPosition(1095.f, 675.f);
        btnEndTurn.text.setFillColor(sf::Color::Black);

        btnExit.rect.setSize({ 80.f, 30.f });
        btnExit.rect.setPosition(1180.f, 10.f);
        btnExit.rect.setFillColor(sf::Color(180, 80, 80));
        btnExit.text.setFont(font);
        btnExit.text.setCharacterSize(14);
        btnExit.text.setString("Exit");
        btnExit.text.setPosition(1198.f, 14.f);
        btnExit.text.setFillColor(sf::Color::White);
    }

    void setupPlayerAndDeck()
    {
        world.player.name = "Hero";
        world.player.maxHp = playerMaxHp;
        world.player.hp = playerMaxHp;
        world.player.armor = 0;
        world.player.shieldElement = ElementType::None;
        world.player.elem = ElementType::None;

        std::vector<std::shared_ptr<Ability>> cards;

        cards.push_back(std::make_shared<SimpleAbility>("Fire Strike", 5, AbilityKind::Damage, fireElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Fire Blast", 5, AbilityKind::Damage, fireElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Fire Heal", 2, AbilityKind::Heal, fireElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Fire Shield", 4, AbilityKind::Shield, fireElem, singleMethod));

        cards.push_back(std::make_shared<SimpleAbility>("Water Strike", 5, AbilityKind::Damage, waterElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Water Blast", 5, AbilityKind::Damage, waterElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Water Heal", 2, AbilityKind::Heal, waterElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Water Shield", 4, AbilityKind::Shield, waterElem, singleMethod));

        cards.push_back(std::make_shared<SimpleAbility>("Earth Strike", 5, AbilityKind::Damage, earthElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Earth Blast", 5, AbilityKind::Damage, earthElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Earth Heal", 2, AbilityKind::Heal, earthElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Earth Shield", 4, AbilityKind::Shield, earthElem, singleMethod));

        cards.push_back(std::make_shared<SimpleAbility>("Nature Strike", 5, AbilityKind::Damage, natureElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Nature Blast", 5, AbilityKind::Damage, natureElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Nature Heal", 2, AbilityKind::Heal, natureElem, singleMethod));
        cards.push_back(std::make_shared<SimpleAbility>("Nature Shield", 4, AbilityKind::Shield, natureElem, singleMethod));

        ownedCards.clear();
        for (auto& c : cards)
            ownedCards.push_back(Card{ c });

        resetBattleDeck();

        cardWidgets.resize(6);
        for (int i = 0; i < 6; ++i)
        {
            cardWidgets[i].rect.setSize({ 160.f, 90.f });
            cardWidgets[i].rect.setFillColor(sf::Color(230, 230, 230));
            cardWidgets[i].rect.setOutlineThickness(2.f);
            cardWidgets[i].rect.setOutlineColor(sf::Color::Black);
            cardWidgets[i].rect.setPosition(40.f + i * 200.f, 560.f);

            cardWidgets[i].name.setFont(font);
            cardWidgets[i].name.setCharacterSize(14);
            cardWidgets[i].name.setFillColor(sf::Color::Black);
            cardWidgets[i].name.setPosition(48.f + i * 200.f, 570.f);

            cardWidgets[i].desc.setFont(font);
            cardWidgets[i].desc.setCharacterSize(11);
            cardWidgets[i].desc.setFillColor(sf::Color::Black);
            cardWidgets[i].desc.setPosition(48.f + i * 200.f, 592.f);
        }

        rewardPool.clear();

        rewardPool.push_back(std::make_shared<FireAOEDamage>(fireElem, areaMethod));
        rewardPool.push_back(std::make_shared<SimpleAbility>("Prayer", 4, AbilityKind::Heal, waterElem, chainLeftMethod));
        rewardPool.push_back(std::make_shared<SimpleAbility>("Perseverance", 6, AbilityKind::Shield, earthElem, chainRightMethod));
        rewardPool.push_back(std::make_shared<SimpleAbility>("Second wind", 3, AbilityKind::Heal, natureElem, areaMethod));
        rewardPool.push_back(std::make_shared<SimpleAbility>("Hell of a deal", 4, AbilityKind::Heal, fireElem, singleMethod));
        rewardPool.push_back(std::make_shared<SimpleAbility>("Water obstacles", 5, AbilityKind::Shield, waterElem, areaMethod));

        rewardPool.push_back(std::make_shared<SimpleAbility>("Icebolt", 20, AbilityKind::Damage, waterElem, singleMethod));
        rewardPool.push_back(std::make_shared<SimpleAbility>("Invasion", 15, AbilityKind::Damage, natureElem, chainLeftMethod));
        rewardPool.push_back(std::make_shared<SimpleAbility>("Avalanche", 15, AbilityKind::Damage, earthElem, chainRightMethod));
        rewardPool.push_back(std::make_shared<SimpleAbility>("Tsunami", 10, AbilityKind::Damage, waterElem, areaMethod));
    }

    void resetBattleDeck()
    {
        deck.setShuffled(ownedCards);
        hand.clear();
    }

    void clearPlayerTempShield()
    {
        clearTempShield(world.player);
    }

    void clearEnemyTempShields()
    {
        for (auto& e : world.enemies)
            clearTempShield(e);
    }

    void applyPlayerTimedEffects()
    {
        clearPlayerTempShield();

        std::vector<TimedStatus> next;
        for (auto st : world.playerStatuses)
        {
            float mult = timedMultiplier(st.method, st.age);
            int val = static_cast<int>(std::round(st.baseValue * mult));

            if (st.kind == AbilityKind::Heal)
            {
                float healMult = healMultiplierFromAttacks(st.element, world.lastEnemyAttackElements);
                world.player.heal(static_cast<int>(std::round(val * healMult)));
            }
            else if (st.kind == AbilityKind::Shield)
            {
                world.player.armor += val;
                world.player.shieldElement = st.element;
            }

            st.age++;
            st.ticksLeft--;

            if (st.ticksLeft > 0)
                next.push_back(st);
        }

        world.playerStatuses = std::move(next);
    }

    void beginPlayerTurn()
    {
        applyPlayerTimedEffects();
        world.lastPlayerAttackElement = ElementType::None;
        playCountThisTurn = 0;
        selectedAbilityForPlay = nullptr;
        selectedTargetIndex = -1;
        refillHand();
    }

    void refillHand()
    {
        while (static_cast<int>(hand.size()) < 6)
        {
            Card c = deck.drawOne();
            if (!c.ability) break;
            hand.push_back(c);
        }
    }

    std::vector<ElementType> randomEnemyLineup(int count)
    {
        std::vector<ElementType> pool = {
            ElementType::Fire,
            ElementType::Water,
            ElementType::Earth,
            ElementType::Nature
        };

        shuffle_vec(pool);

        std::vector<ElementType> result;
        for (int i = 0; i < count; ++i)
        {
            if (i < static_cast<int>(pool.size()))
                result.push_back(pool[i]);
            else
                result.push_back(static_cast<ElementType>(roll_int(0, 3)));
        }
        return result;
    }

    void generateEnemyIntents()
    {
        for (auto& e : world.enemies)
            e.rollIntent();
    }

    void startLevel(int lvl)
    {
        currentLevel = lvl;

        world.enemies.clear();
        world.lastEnemyAttackElement = ElementType::None;
        world.lastPlayerAttackElement = ElementType::None;
        world.lastEnemyAttackElements.clear();
        world.playerStatuses.clear();
        clearPlayerTempShield();

        resetBattleDeck();

        const LevelConfig& cfg = levelConfigs[std::max(1, std::min(5, lvl)) - 1];
        std::vector<ElementType> lineup = randomEnemyLineup(cfg.enemyCount);

        world.enemies.reserve(cfg.enemyCount);
        for (int i = 0; i < cfg.enemyCount; ++i)
            world.enemies.push_back(ElementalEnemy::create(lineup[i], lvl, cfg));

        generateEnemyIntents();
        beginPlayerTurn();
    }

    sf::Color elementColor(ElementType e) const
    {
        if (e == ElementType::Fire) return sf::Color(220, 90, 60);
        if (e == ElementType::Water) return sf::Color(80, 140, 220);
        if (e == ElementType::Earth) return sf::Color(160, 120, 80);
        if (e == ElementType::Nature) return sf::Color(120, 200, 100);
        return sf::Color::White;
    }

    void drawText(const std::string& str, float x, float y, unsigned size = 16, sf::Color color = sf::Color::White)
    {
        sf::Text t;
        t.setFont(font);
        t.setCharacterSize(size);
        t.setFillColor(color);
        t.setString(str);
        t.setPosition(x, y);
        window.draw(t);
    }

    void drawBackground()
    {
        sf::RectangleShape bg({ 1280.f, 720.f });
        bg.setFillColor(sf::Color(28, 30, 40));
        window.draw(bg);

        sf::RectangleShape haze({ 1280.f, 220.f });
        haze.setPosition(0.f, 500.f);
        haze.setFillColor(sf::Color(18, 20, 28));
        window.draw(haze);

        sf::CircleShape moon(55.f);
        moon.setFillColor(sf::Color(120, 130, 160, 40));
        moon.setPosition(1040.f, 35.f);
        window.draw(moon);

        const sf::Vector2f stars[] = {
            {70.f, 60.f}, {120.f, 110.f}, {180.f, 50.f}, {250.f, 90.f},
            {360.f, 40.f}, {470.f, 120.f}, {600.f, 70.f}, {720.f, 100.f},
            {840.f, 55.f}, {940.f, 130.f}, {1180.f, 50.f}
        };

        for (auto& p : stars)
        {
            sf::CircleShape star(2.5f);
            star.setPosition(p);
            star.setFillColor(sf::Color(255, 255, 255, 150));
            window.draw(star);
        }
    }

    void drawLegend()
    {
        sf::RectangleShape panel({ 210.f, 210.f });
        panel.setPosition(1030.f, 280.f);
        panel.setFillColor(sf::Color(25, 25, 35, 220));
        panel.setOutlineThickness(1.f);
        panel.setOutlineColor(sf::Color(120, 120, 140));
        window.draw(panel);

        drawText("Element scheme", 1065.f, 290.f, 16, sf::Color::White);

        auto row = [&](int y, ElementType a, ElementType b)
            {
                sf::RectangleShape left({ 14.f, 14.f });
                left.setPosition(1050.f, static_cast<float>(y));
                left.setFillColor(elementColor(a));
                window.draw(left);

                drawText(">", 1075.f, static_cast<float>(y) - 2.f, 16, sf::Color::White);

                sf::RectangleShape right({ 14.f, 14.f });
                right.setPosition(1100.f, static_cast<float>(y));
                right.setFillColor(elementColor(b));
                window.draw(right);

                drawText(elementToString(a) + " > " + elementToString(b), 1130.f, static_cast<float>(y) - 2.f, 15, sf::Color::White);
            };

        row(325, ElementType::Fire, ElementType::Nature);
        row(355, ElementType::Nature, ElementType::Earth);
        row(385, ElementType::Earth, ElementType::Water);
        row(415, ElementType::Water, ElementType::Fire);

        drawText("x2 / x0.5", 1085.f, 440.f, 14, sf::Color(200, 200, 210));
    }

    const sf::Texture* textureForElement(ElementType e) const
    {
        if (e == ElementType::Fire && enemyTextureLoaded[0]) return &enemyTextures[0];
        if (e == ElementType::Water && enemyTextureLoaded[1]) return &enemyTextures[1];
        if (e == ElementType::Earth && enemyTextureLoaded[2]) return &enemyTextures[2];
        if (e == ElementType::Nature && enemyTextureLoaded[3]) return &enemyTextures[3];
        return nullptr;
    }

    void drawHero()
    {
        sf::CircleShape shadow(40.f);
        shadow.setScale(1.7f, 0.45f);
        shadow.setFillColor(sf::Color(0, 0, 0, 90));
        shadow.setPosition(48.f, 406.f);
        window.draw(shadow);

        if (heroTextureLoaded)
        {
            sf::Sprite hero(heroTexture);
            auto s = heroTexture.getSize();
            hero.setOrigin(s.x / 2.f, s.y / 2.f);
            hero.setScale(0.20f, 0.20f);
            hero.setPosition(115.f, 335.f);
            window.draw(hero);
        }
        else
        {
            sf::RectangleShape playerRect({ 150.f, 210.f });
            playerRect.setPosition(40.f, 220.f);
            playerRect.setFillColor(sf::Color(80, 120, 220));
            playerRect.setOutlineThickness(3.f);
            playerRect.setOutlineColor(sf::Color(180, 200, 255));
            window.draw(playerRect);
        }

        drawText("HERO", 88.f, 200.f, 20, sf::Color::White);
    }

    void drawPlayerStatuses()
    {
        if (world.playerStatuses.empty()) return;

        sf::RectangleShape panel({ 280.f, 70.f });
        panel.setPosition(40.f, 430.f);
        panel.setFillColor(sf::Color(25, 25, 35, 180));
        panel.setOutlineThickness(1.f);
        panel.setOutlineColor(sf::Color(120, 120, 140));
        window.draw(panel);

        drawText("Active effects", 50.f, 438.f, 14, sf::Color::White);

        for (size_t i = 0; i < world.playerStatuses.size() && i < 5; ++i)
        {
            const auto& st = world.playerStatuses[i];

            sf::RectangleShape box({ 34.f, 34.f });
            box.setPosition(50.f + i * 46.f, 458.f);
            box.setFillColor(elementColor(st.element));
            box.setOutlineThickness(2.f);
            box.setOutlineColor(st.kind == AbilityKind::Heal ? sf::Color::White : sf::Color::Black);
            window.draw(box);

            int tickValue = static_cast<int>(std::round(st.baseValue * timedMultiplier(st.method, st.age)));
            drawText(std::to_string(tickValue),
                box.getPosition().x + 6.f,
                box.getPosition().y + 4.f,
                12,
                sf::Color::Black);

            drawText(std::to_string(st.ticksLeft),
                box.getPosition().x + 12.f,
                box.getPosition().y + 18.f,
                11,
                sf::Color::Black);
        }
    }

    void drawEnemySlot(const ElementalEnemy& e, float x, float y, int index)
    {
        if (!e.alive()) return;

        if (const sf::Texture* tex = textureForElement(e.elem))
        {
            sf::Sprite sp(*tex);
            auto s = tex->getSize();
            sp.setOrigin(s.x / 2.f, s.y / 2.f);
            sp.setScale(0.30f, 0.30f);
            sp.setPosition(x + 60.f, y + 78.f);
            window.draw(sp);
        }
        else
        {
            sf::RectangleShape rect({ 120.f, 140.f });
            rect.setPosition(x, y);
            rect.setFillColor(elementColor(e.elem));
            rect.setOutlineThickness(2.f);
            rect.setOutlineColor(sf::Color::Black);
            window.draw(rect);
        }

        drawText(e.intent.label, x - 4.f, y - 42.f, 13, sf::Color::Yellow);
        drawText(e.name, x + 4.f, y - 20.f, 16, sf::Color::White);

        float perc = static_cast<float>(e.hp) / static_cast<float>(e.maxHp);

        sf::RectangleShape hpBack({ 100.f, 12.f });
        hpBack.setPosition(x + 10.f, y + 10.f);
        hpBack.setFillColor(sf::Color(50, 50, 50));
        window.draw(hpBack);

        sf::RectangleShape hpBar({ 100.f * perc, 12.f });
        hpBar.setPosition(x + 10.f, y + 10.f);
        hpBar.setFillColor(sf::Color(220, 60, 60));
        window.draw(hpBar);

        drawText(std::to_string(e.hp), x + 10.f, y + 28.f, 14);
        drawText("A:" + std::to_string(e.armor), x + 10.f, y + 45.f, 14);

        if (selectedTargetIndex == index)
        {
            sf::RectangleShape hl({ 124.f, 144.f });
            hl.setPosition(x - 2.f, y - 2.f);
            hl.setFillColor(sf::Color::Transparent);
            hl.setOutlineThickness(3.f);
            hl.setOutlineColor(sf::Color::Yellow);
            window.draw(hl);
        }
    }

    void drawUI()
    {
        window.clear();
        drawBackground();

        drawText("Level: " + std::to_string(currentLevel) + " / 5", 40.f, 10.f, 18, sf::Color(230, 230, 230));
        drawText("HP: " + std::to_string(world.player.hp) + " / " + std::to_string(world.player.maxHp), 40.f, 35.f, 18);
        drawText("Armor: " + std::to_string(world.player.armor), 40.f, 60.f, 18);
        drawText("Deck: " + std::to_string(deck.draw.size()), 40.f, 90.f, 16);
        drawText("Discard: " + std::to_string(deck.discard.size()), 140.f, 90.f, 16);
        drawText("Hand: " + std::to_string(hand.size()), 260.f, 90.f, 16);
        drawText("Plays: " + std::to_string(playCountThisTurn) + " / " + std::to_string(maxPlaysPerTurn), 400.f, 90.f, 16);

        drawHero();
        drawPlayerStatuses();

        int n = static_cast<int>(world.enemies.size());
        float baseX = 660.f;
        float startY = 130.f;
        float gapX = 160.f;

        for (int i = 0; i < n; ++i)
        {
            float ex = baseX + (i - (n - 1) / 2.0f) * gapX;
            drawEnemySlot(world.enemies[i], ex, startY, i);
        }

        for (size_t i = 0; i < hand.size() && i < cardWidgets.size(); ++i)
        {
            auto& w = cardWidgets[i];
            w.card = hand[i];

            auto et = w.card.ability->element->type();
            if (et == ElementType::Fire) w.rect.setFillColor(sf::Color(250, 200, 190));
            else if (et == ElementType::Water) w.rect.setFillColor(sf::Color(190, 220, 250));
            else if (et == ElementType::Earth) w.rect.setFillColor(sf::Color(220, 205, 170));
            else if (et == ElementType::Nature) w.rect.setFillColor(sf::Color(200, 240, 200));

            if (selectedAbilityForPlay == w.card.ability) w.rect.setOutlineColor(sf::Color::Yellow);
            else w.rect.setOutlineColor(sf::Color::Black);

            window.draw(w.rect);

            w.name.setString(w.card.ability->name);
            w.desc.setString(w.card.ability->description());

            window.draw(w.name);
            window.draw(w.desc);

            std::string kind;
            if (w.card.ability->kind == AbilityKind::Damage) kind = "D";
            else if (w.card.ability->kind == AbilityKind::Heal) kind = "H";
            else kind = "S";

            drawText("[" + kind + "]", w.rect.getPosition().x + 118.f, w.rect.getPosition().y + 6.f, 12, sf::Color::Black);

            drawText("v:" + std::to_string(w.card.ability->baseValue),
                w.rect.getPosition().x + 8.f,
                w.rect.getPosition().y + 66.f,
                14,
                sf::Color::Black);
        }

        window.draw(btnEndTurn.rect);
        window.draw(btnEndTurn.text);

        window.draw(btnExit.rect);
        window.draw(btnExit.text);

        drawLegend();

        if (inReward)
        {
            sf::RectangleShape panel({ 600.f, 220.f });
            panel.setPosition(340.f, 200.f);
            panel.setFillColor(sf::Color(40, 40, 40, 230));
            window.draw(panel);

            drawText("Choose one reward:", 420.f, 210.f, 18);

            for (int i = 0; i < 3; ++i)
            {
                sf::RectangleShape r({ 160.f, 140.f });
                r.setPosition(420.f + i * 200.f, 260.f);
                r.setFillColor(sf::Color(200, 200, 220));
                window.draw(r);

                if (i < static_cast<int>(rewardChoices.size()))
                {
                    drawText(rewardChoices[i]->name, r.getPosition().x + 6.f, r.getPosition().y + 6.f, 14, sf::Color::Black);
                    drawText(rewardChoices[i]->description(), r.getPosition().x + 6.f, r.getPosition().y + 28.f, 11, sf::Color::Black);
                    drawText("v:" + std::to_string(rewardChoices[i]->baseValue), r.getPosition().x + 6.f, r.getPosition().y + 52.f, 14, sf::Color::Black);
                }
            }
        }

        if (showWin)
        {
            sf::RectangleShape p({ 600.f, 120.f });
            p.setPosition(340.f, 300.f);
            p.setFillColor(sf::Color(40, 40, 40, 200));
            window.draw(p);
            drawText("You win! Demo complete.", 470.f, 340.f, 22);
        }

        if (showLose)
        {
            sf::RectangleShape p({ 600.f, 120.f });
            p.setPosition(340.f, 300.f);
            p.setFillColor(sf::Color(70, 30, 30, 200));
            window.draw(p);
            drawText("You died. Demo over.", 500.f, 340.f, 22);
        }

        window.display();
    }

    void onCardClicked(int idx)
    {
        if (idx < 0 || idx >= static_cast<int>(hand.size())) return;

        if (selectedAbilityForPlay == hand[idx].ability)
        {
            selectedAbilityForPlay = nullptr;
            selectedTargetIndex = -1;
        }
        else
        {
            selectedAbilityForPlay = hand[idx].ability;
            selectedTargetIndex = -1;
        }
    }

    void onEnemyClicked(int enemyIndex)
    {
        if (selectedAbilityForPlay == nullptr)
        {
            selectedTargetIndex = enemyIndex;
            return;
        }

        if (playCountThisTurn >= maxPlaysPerTurn) return;
        if (selectedAbilityForPlay->kind != AbilityKind::Damage) return;

        int foundIdx = -1;
        for (int i = 0; i < static_cast<int>(hand.size()); ++i)
        {
            if (hand[i].ability == selectedAbilityForPlay)
            {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx == -1) return;

        world.lastPlayerAttackElement = selectedAbilityForPlay->element->type();

        MethodKind mk = selectedAbilityForPlay->method->methodKind();
        std::vector<int> targets;

        if (mk == MethodKind::Single || mk == MethodKind::ChainLeft || mk == MethodKind::ChainRight)
            targets.push_back(enemyIndex);

        selectedAbilityForPlay->use(world, targets);

        deck.discard.push_back(hand[foundIdx]);
        hand.erase(hand.begin() + foundIdx);

        playCountThisTurn++;
        selectedAbilityForPlay = nullptr;
        selectedTargetIndex = -1;
    }

    void onPlayerSelfClicked()
    {
        if (!selectedAbilityForPlay) return;
        if (playCountThisTurn >= maxPlaysPerTurn) return;

        if (selectedAbilityForPlay->kind == AbilityKind::Heal || selectedAbilityForPlay->kind == AbilityKind::Shield)
        {
            int foundIdx = -1;
            for (int i = 0; i < static_cast<int>(hand.size()); ++i)
            {
                if (hand[i].ability == selectedAbilityForPlay)
                {
                    foundIdx = i;
                    break;
                }
            }

            if (foundIdx != -1)
            {
                std::vector<int> empty;
                selectedAbilityForPlay->use(world, empty);

                deck.discard.push_back(hand[foundIdx]);
                hand.erase(hand.begin() + foundIdx);

                playCountThisTurn++;
                selectedAbilityForPlay = nullptr;
            }
        }
    }

    void enemyPhase()
    {
        clearEnemyTempShields();
        world.lastEnemyAttackElements.clear();

        for (size_t i = 0; i < world.enemies.size(); ++i)
        {
            ElementalEnemy& en = world.enemies[i];
            if (!en.alive()) continue;

            const auto& intent = en.intent;

            if (intent.kind == EnemyIntentKind::Attack)
            {
                applyDamage(world.player, intent.value, en.elem);
                world.lastEnemyAttackElement = en.elem;
                world.lastEnemyAttackElements.push_back(en.elem);
            }
            else if (intent.kind == EnemyIntentKind::Shield)
            {
                en.armor += intent.value;
                en.shieldElement = en.elem;
            }
            else if (intent.kind == EnemyIntentKind::Heal)
            {
                float mult = healMultiplier(en.elem, world.lastPlayerAttackElement);
                en.heal(static_cast<int>(std::round(intent.value * mult)));
            }
        }

        world.lastPlayerAttackElement = ElementType::None;
        generateEnemyIntents();
    }

    bool allEnemiesDead() const
    {
        for (const auto& e : world.enemies)
            if (e.alive()) return false;
        return true;
    }

    void checkEndOfBattle()
    {
        if (!allEnemiesDead()) return;

        if (currentLevel < 5)
        {
            std::vector<int> idx(rewardPool.size());
            for (size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);
            shuffle_vec(idx);

            rewardChoices.clear();
            for (int i = 0; i < 3 && i < static_cast<int>(idx.size()); ++i)
                rewardChoices.push_back(rewardPool[idx[i]]);

            inReward = true;
        }
        else
        {
            showWin = true;
        }
    }

    void chooseReward(int which)
    {
        if (which < 0 || which >= static_cast<int>(rewardChoices.size())) return;

        ownedCards.push_back(Card{ rewardChoices[which] });
        inReward = false;
        startLevel(currentLevel + 1);
    }

    void update()
    {
        if (showWin || showLose) return;
        if (world.player.hp <= 0)
        {
            showLose = true;
            return;
        }

        if (!inReward)
            checkEndOfBattle();
    }

    void run()
    {
        while (window.isOpen())
        {
            sf::Event ev;
            while (window.pollEvent(ev))
            {
                if (ev.type == sf::Event::Closed)
                    window.close();

                if (ev.type == sf::Event::MouseButtonPressed)
                {
                    sf::Vector2f mp(static_cast<float>(ev.mouseButton.x), static_cast<float>(ev.mouseButton.y));

                    if (btnExit.contains(mp))
                    {
                        window.close();
                        break;
                    }

                    if (showWin || showLose)
                        continue;

                    if (inReward)
                    {
                        for (int i = 0; i < 3; ++i)
                        {
                            sf::FloatRect r(420.f + i * 200.f, 260.f, 160.f, 140.f);
                            if (r.contains(mp))
                            {
                                chooseReward(i);
                                break;
                            }
                        }
                        continue;
                    }

                    if (btnEndTurn.contains(mp))
                    {
                        playCountThisTurn = 0;
                        selectedAbilityForPlay = nullptr;
                        selectedTargetIndex = -1;

                        enemyPhase();

                        if (!showLose)
                            beginPlayerTurn();

                        continue;
                    }

                    for (int i = 0; i < static_cast<int>(cardWidgets.size()); ++i)
                    {
                        if (cardWidgets[i].rect.getGlobalBounds().contains(mp))
                        {
                            if (i < static_cast<int>(hand.size()))
                                onCardClicked(i);
                            break;
                        }
                    }

                    if (sf::FloatRect(40.f, 220.f, 150.f, 210.f).contains(mp))
                    {
                        onPlayerSelfClicked();
                    }

                    int n = static_cast<int>(world.enemies.size());
                    float baseX = 660.f;
                    float startY = 130.f;
                    float gapX = 160.f;

                    for (int i = 0; i < n; ++i)
                    {
                        sf::FloatRect r(baseX + (i - (n - 1) / 2.0f) * gapX, startY, 120.f, 140.f);
                        if (r.contains(mp))
                        {
                            onEnemyClicked(i);
                            break;
                        }
                    }
                }
            }

            update();
            drawUI();
        }
    }
};

int main()
{
    Game g;
    g.run();
    return 0;
}