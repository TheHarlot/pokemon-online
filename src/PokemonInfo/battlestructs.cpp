#include "battlestructs.h"
#include "networkstructs.h"
#include "movesetchecker.h"
#include "../Utilities/otherwidgets.h"

QString ChallengeInfo::clauseText[] =
{
    QObject::tr("Sleep Clause"),
    QObject::tr("Freeze Clause"),
    QObject::tr("Disallow Spects"),
    QObject::tr("Item Clause"),
    QObject::tr("Challenge Cup"),
    QObject::tr("No Timeout"),
    QObject::tr("Species Clause"),
    QObject::tr("Wifi Battle"),
    QObject::tr("Self-KO Clause")
};

QString ChallengeInfo::clauseBattleText[] =
{
    QObject::tr("Sleep Clause prevented the sleep inducing effect of the move from working."),
    QObject::tr("Freeze Clause prevented the freezing effect of the move from working."),
    QObject::tr(""),
    QObject::tr(""),
    QObject::tr(""),
    QObject::tr("The battle ended by timeout."),
    QObject::tr(""),
    QObject::tr(""),
    QObject::tr("The Self-KO Clause acted as a tiebreaker.")
};

QString ChallengeInfo::clauseDescription[] =
{
    QObject::tr("You can not put more than one Pokemon of the opposing team to sleep at the same time."),
    QObject::tr("You can not freeze more than one Pokemon of the opposing team at the same time."),
    QObject::tr("Nobody can watch your battle."),
    QObject::tr("No more than one of the same items is allowed per team."),
    QObject::tr("Random teams are given to trainers."),
    QObject::tr("No time limit for playing."),
    QObject::tr("One player cannot have more than one of the same pokemon per team."),
    QObject::tr("At the beginning of the battle, you can see the opponent's team and rearrange yours accordingly."),
    QObject::tr("The one who causes a tie (Recoil, Explosion, Destinybond, ...) loses the battle.")
};

QString ChallengeInfo::modeText[] =
{
    QObject::tr("Singles", "Mode"),
    QObject::tr("Doubles", "Mode"),
    QObject::tr("Triples", "Mode"),
    QObject::tr("Rotation", "Mode")
};

BattleMove::BattleMove()
{
    num() = 0;
    PP() = 0;
    totalPP() = 0;
}

void BattleMove::load(int gen) {
    PP() = MoveInfo::PP(num(), gen)*(num() == Move::TrumpCard ? 5 :8)/5; /* 3 PP-ups */
    totalPP() = PP();
}

QDataStream & operator >> (QDataStream &in, BattleMove &mo)
{
    in >> mo.num() >> mo.PP() >> mo.totalPP();

    return in;
}

QDataStream & operator << (QDataStream &out, const BattleMove &mo)
{
    out << mo.num() << mo.PP() << mo.totalPP();

    return out;
}

PokeBattle::PokeBattle()
{
    num() = Pokemon::NoPoke;
    ability() = 0;
    item() = 0;
    gender() = 0;
    fullStatus() = 1;
    lifePoints() = 0;
    totalLifePoints() = 1;
    level() = 100;
    happiness() = 255;
    itemUsed() = 0;
    itemUsedTurn() = 0;

    for (int i = 0; i < 6; i++) {
        dvs() << 31;
    }
    for (int i = 0; i < 6; i++) {
        evs() << 80;
    }
}

const BattleMove & PokeBattle::move(int i) const
{
    return m_moves[i];
}

BattleMove & PokeBattle::move(int i)
{
    return m_moves[i];
}

quint16 PokeBattle::normalStat(int stat) const
{
    return normal_stats[stat-1];
}

void PokeBattle::setNormalStat(int stat, quint16 i)
{
    normal_stats[stat-1] = i;
}

void PokeBattle::init(PokePersonal &poke)
{
    /* Checks num, ability, moves, item */
    poke.runCheck();

    num() = poke.num();

    if (num() == Pokemon::NoPoke)
        return;


    PokeGeneral p;
    p.gen() = poke.gen();
    p.num() = poke.num();
    p.load();

    QNickValidator v(NULL);

    happiness() = poke.happiness();

    item() = poke.item();
    ability() = poke.ability();

    if (item() == Item::GriseousOrb && num() != Pokemon::Giratina_O && p.gen() <= 4) {
        item() = 0;
    } else if (num() == Pokemon::Giratina_O && item() != Item::GriseousOrb) {
        num() = Pokemon::Giratina;
    }

    if (PokemonInfo::OriginalForme(num()) == Pokemon::Arceus) {
        if (ItemInfo::isPlate(item())) {
            num().subnum = ItemInfo::PlateType(item());
        } else {
            num().subnum = 0;
        }
    }

    Pokemon::uniqueId ori = PokemonInfo::OriginalForme(num());

    if (ori == Pokemon::Castform || ori == Pokemon::Cherrim || ori == Pokemon::Hihidaruma || ori == Pokemon::Meloia) {
        num().subnum = 0;
    }

    nick() = (v.validate(poke.nickname()) == QNickValidator::Acceptable && poke.nickname().length() <= 12) ? poke.nickname() : PokemonInfo::Name(num());

    if (GenderInfo::Possible(poke.gender(), p.genderAvail())) {
        gender() = poke.gender();
    } else {
        gender() = GenderInfo::Default(p.genderAvail());
    }

    shiny() = poke.shiny();
    level() = std::min(100, std::max(int(poke.level()), 1));

    nature() = std::min(NatureInfo::NumberOfNatures() - 1, std::max(0, int(poke.nature())));

    int curs = 0;
    QSet<int> taken_moves;
    
    for (int i = 0; i < 4; i++) {
        if (!taken_moves.contains(poke.move(i)) && poke.move(i) != 0) {
            taken_moves.insert(poke.move(i));
            move(curs).num() = poke.move(i);
            move(curs).load(poke.gen());
            ++curs;
        }
    }

    if (move(0).num() == 0) {
		num() = 0;
		return;
    }

    dvs().clear();
    for (int i = 0; i < 6; i++) {
        dvs() << std::min(std::max(poke.DV(i), quint8(0)),quint8(p.gen() <= 2? 15: 31));
    }

    evs().clear();
    for (int i = 0; i < 6; i++) {
        evs() << std::min(std::max(poke.EV(i), quint8(0)), quint8(255));
    }

    if (p.gen() >= 3) {
        int sum = 0;
        for (int i = 0; i < 6; i++) {
            //Arceus
            if (PokemonInfo::OriginalForme(num()) == Pokemon::Arceus && evs()[i] > 100 && p.gen() < 5) evs()[i] = 100;
            sum += evs()[i];
            if (sum > 510) {
                evs()[i] -= (sum-510);
                sum = 510;
            }
        }
    }

    updateStats(p.gen());
}

void PokeBattle::updateStats(int gen)
{
    totalLifePoints() = std::max(PokemonInfo::FullStat(num(), gen, nature(), Hp, level(), dvs()[Hp], evs()[Hp]),1);
    setLife(totalLifePoints());

    for (int i = 0; i < 5; i++) {
        normal_stats[i] = PokemonInfo::FullStat(num(), gen, nature(), i+1, level(), dvs()[i+1], evs()[i+1]);
    }
}

QDataStream & operator >> (QDataStream &in, PokeBattle &po)
{
    in >> po.num() >> po.nick() >> po.totalLifePoints() >> po.lifePoints() >> po.gender() >> po.shiny() >> po.level() >> po.item() >> po.ability()
       >> po.happiness();

    for (int i = 0; i < 5; i++) {
        quint16 st;
        in >> st;
        po.setNormalStat(i+1, st);
    }

    for (int i = 0; i < 4; i++) {
        in >> po.move(i);
    }

    for (int i = 0; i < 6; i++) {
        in >> po.evs()[i];
    }

    for (int i = 0; i < 6; i++) {
        in >> po.dvs()[i];
    }

    return in;
}

QDataStream & operator << (QDataStream &out, const PokeBattle &po)
{
    out << po.num() << po.nick() << po.totalLifePoints() << po.lifePoints() << po.gender() << po.shiny() << po.level() << po.item() << po.ability()
        << po.happiness();

    for (int i = 0; i < 5; i++) {
        out << po.normalStat(i+1);
    }

    for (int i = 0; i < 4; i++) {
        out << po.move(i);
    }

    for (int i = 0; i < 6; i++) {
        out << po.evs()[i];
    }

    for (int i = 0; i < 6; i++) {
        out << po.dvs()[i];
    }

    return out;
}

ShallowBattlePoke::ShallowBattlePoke()
{
}

ShallowBattlePoke::ShallowBattlePoke(const PokeBattle &p)
{
    init(p);
}

void ShallowBattlePoke::init(const PokeBattle &poke)
{
    nick() = poke.nick();
    fullStatus() = poke.fullStatus();
    num() = poke.num();
    shiny() = poke.shiny();
    gender() = poke.gender();
    setLifePercent( (poke.lifePoints() * 100) / poke.totalLifePoints() );
    if (lifePercent() == 0 && poke.lifePoints() > 0) {
        setLifePercent(1);
    }
    level() = poke.level();
}

int ShallowBattlePoke::status() const
{
    if (fullStatus() & (1 << Pokemon::Koed))
        return Pokemon::Koed;
    return intlog2(fullStatus() & (0x3F));
}

void ShallowBattlePoke::changeStatus(int status)
{
    /* Clears past status */
    fullStatus() = fullStatus() & ~( (1 << Pokemon::Koed) | 0x3F);
    /* Adds new status */
    fullStatus() = fullStatus() | (1 << status);
}

void ShallowBattlePoke::addStatus(int status)
{
    if (status <= Pokemon::Poisoned || status == Pokemon::Koed) {
        changeStatus(status);
        return;
    }

    fullStatus() = fullStatus() | (1 << status);
}

void ShallowBattlePoke::removeStatus(int status)
{
    fullStatus() = fullStatus() & ~(1 << status);
}


QDataStream & operator >> (QDataStream &in, ShallowBattlePoke &po)
{
    in >> po.num() >> po.nick() >> po.lifePercent() >> po.fullStatus() >> po.gender() >> po.shiny() >> po.level();

    return in;
}

QDataStream & operator << (QDataStream &out, const ShallowBattlePoke &po)
{
    out << po.num() << po.nick() << po.lifePercent() << po.fullStatus() << po.gender() << po.shiny() << po.level();

    return out;
}

TeamBattle::TeamBattle() : gen(GEN_MAX)
{
    for (int i = 0; i < 6; i++) {
        m_indexes[i] = i;
    }
}

TeamBattle::TeamBattle(TeamInfo &other)
{
    resetIndexes();

    name = other.name;
    info = other.info;
    gen = other.gen;

    if (gen < GEN_MIN || gen > GEN_MAX) {
        gen = GEN_MAX;
    }

    int curs = 0;
    for (int i = 0; i < 6; i++) {
        poke(curs).init(other.pokemon(i));
        if (poke(curs).num() != 0) {
            ++curs;
        }
    }
}

void TeamBattle::resetIndexes()
{
    for (int i = 0; i < 6; i++) {
        m_indexes[i] = i;
    }
}

void TeamBattle::switchPokemon(int pok1, int pok2)
{
    std::swap(m_indexes[pok1],m_indexes[pok2]);
}

bool TeamBattle::invalid() const
{
    return poke(0).num() == Pokemon::NoPoke;
}

void TeamBattle::generateRandom(int gen)
{
    QList<Pokemon::uniqueId> pokes;
    for (int i = 0; i < 6; i++) {
        while(1) {
            Pokemon::uniqueId num = PokemonInfo::getRandomPokemon(gen);
            if (pokes.contains(num)) {
                continue ;
            }
            pokes.push_back(num);
            break;
        }

        PokeGeneral g;
        PokeBattle &p = poke(i);

        g.num() = pokes[i];
        p.num() = pokes[i];
        g.gen() = gen;
        g.load();

        if (gen >= GEN_MIN_ABILITIES) {
            p.ability() = g.abilities().ab(true_rand()%3);
            /* In case the pokemon has less than 3 abilities, ability 1 has 2/3 of being chosen. Fix it. */
            if (p.ability() == 0)
                p.ability() = g.abilities().ab(0);
        }


        if (g.genderAvail() == Pokemon::MaleAndFemaleAvail) {
            p.gender() = true_rand()%2 ? Pokemon::Female : Pokemon::Male;
        } else {
            p.gender() = g.genderAvail();
        }

        if (gen > 2) {
            p.nature() = true_rand()%NatureInfo::NumberOfNatures();
        }

        p.level() = PokemonInfo::LevelBalance(p.num());

        PokePersonal p2;

        for (int i = 0; i < 6; i++) {
            p2.setDV(i, true_rand() % (gen <= 2 ? 16 : 32));
        }

        if (gen <= 2) {
            for (int i = 0; i < 6; i++) {
                p2.setEV(i, 255);
            }
        } else {
            while (p2.EVSum() < 510) {
                int stat = true_rand() % 6;
                p2.setEV(stat, std::min(int(p2.EV(stat)) + (true_rand()%255), long(255)));
            }
        }

        p.dvs().clear();
        p.evs().clear();
        p.dvs() << p2.DV(0) << p2.DV(1) << p2.DV(2) << p2.DV(3) << p2.DV(4) << p2.DV(5);
        p.evs() << p2.EV(0) << p2.EV(1) << p2.EV(2) << p2.EV(3) << p2.EV(4) << p2.EV(5);

        QList<int> moves = g.moves().toList();
        QList<int> movesTaken;
        bool off = false;
        for (int i = 0; i < 4; i++) {
            /* If there are no other moves possible,
               sets the remaining moves to 0 and exit the loop
               (like for weedle) */
            if (moves.size() <= i) {
                for (int j = i; j < 4; j++) {
                    p.move(j).num() = 0;
                    p.move(j).load(gen);
                }
                break;
            }
            while(1) {
                int movenum = moves[true_rand()%moves.size()];
                if (movesTaken.contains(movenum)) {
                    continue;
                }
                if (MoveInfo::Power(movenum, gen) > 0 && movenum != Move::NaturalGift && movenum != Move::Snore && movenum != Move::Fling
                        && !MoveInfo::isOHKO(movenum, gen) && movenum != Move::DreamEater && movenum != Move::SynchroNoise && movenum != Move::FalseSwipe
                        && movenum != Move::Feint)
                    off = true;
                if(i == 3 && !off) {
                    continue;
                }
                movesTaken.push_back(movenum);
                p.move(i).num() = movenum;
                p.move(i).load(gen);
                break;
            }
        }

        if (movesTaken.contains(Move::Return))
            p.happiness() = 255;
        else if (movesTaken.contains(Move::Frustration))
            p.happiness() = 0;
        else
            p.happiness() = true_rand() % 256;

        if (gen >= GEN_MIN_ITEMS)
            p.item() = ItemInfo::Number(ItemInfo::SortedUsefulNames(gen)[true_rand()%ItemInfo::SortedUsefulNames(gen).size()]);

        p.updateStats(gen);
        p.nick() = PokemonInfo::Name(p.num());
        p.fullStatus() = 0;
        p.shiny() = !(true_rand() % 50);
    }
}

PokeBattle & TeamBattle::poke(int i)
{
    return m_pokemons[m_indexes[i]];
}

const PokeBattle & TeamBattle::poke(int i) const
{
    return m_pokemons[m_indexes[i]];
}

int TeamBattle::internalId(const PokeBattle &p) const
{
    for (int i = 0; i < 6; i++) {
        if (m_pokemons + i == &p)
            return i;
    }

    return 0;
}

const PokeBattle &TeamBattle::getByInternalId(int i) const
{
    return m_pokemons[i];
}

QDataStream & operator >> (QDataStream &in, TeamBattle &te)
{
    for (int i = 0; i < 6; i++) {
        in >> te.poke(i);
    }

    return in;
}

QDataStream & operator << (QDataStream &out, const TeamBattle &te)
{
    for (int i = 0; i < 6; i++) {
        out << te.poke(i);
    }

    return out;
}

ShallowShownPoke::ShallowShownPoke()
{

}

void ShallowShownPoke::init(const PokeBattle &b)
{
    item = b.item() != 0;
    num = b.num();
    level = b.level();
    gender = b.gender();

    /* All arceus formes have the same icon */
    if (PokemonInfo::OriginalForme(num) == Pokemon::Arceus) {
        num = Pokemon::Arceus;
    }
}

QDataStream & operator >> (QDataStream &in, ShallowShownPoke &po) {
    in >> po.num >> po.level >> po.gender >> po.item;

    return in;
}

QDataStream & operator << (QDataStream &out, const ShallowShownPoke &po) {
    out << po.num << po.level << po.gender << po.item;

    return out;
}

ShallowShownTeam::ShallowShownTeam(const TeamBattle &t)
{
    for (int i = 0; i < 6; i++) {
        pokemons[i].init(t.poke(i));
    }
}

QDataStream & operator >> (QDataStream &in, ShallowShownTeam &po) {
    for (int i = 0; i < 6; i++) {
        in >> po.poke(i);
    }

    return in;
}

QDataStream & operator << (QDataStream &out, const ShallowShownTeam &po) {
    for (int i = 0; i < 6; i++) {
        out << po.poke(i);
    }

    return out;
}

BattleChoices::BattleChoices()
{
    switchAllowed = true;
    attacksAllowed = true;
    std::fill(attackAllowed, attackAllowed+4, true);
}

void BattleChoices::disableSwitch()
{
    switchAllowed = false;
}

void BattleChoices::disableAttack(int attack)
{
    attackAllowed[attack] = false;
}

void BattleChoices::disableAttacks()
{
    std::fill(attackAllowed, attackAllowed+4, false);
    attacksAllowed = false;
}

BattleChoices BattleChoices::SwitchOnly(quint8 slot)
{
    BattleChoices ret;
    ret.disableAttacks();
    ret.numSlot = slot;

    return ret;
}

QDataStream & operator >> (QDataStream &in, BattleChoices &po)
{
    in >> po.numSlot >> po.switchAllowed >> po.attacksAllowed >> po.attackAllowed[0] >> po.attackAllowed[1] >> po.attackAllowed[2] >> po.attackAllowed[3];
    return in;
}

QDataStream & operator << (QDataStream &out, const BattleChoices &po)
{
    out << po.numSlot << po.switchAllowed << po.attacksAllowed << po.attackAllowed[0] << po.attackAllowed[1] << po.attackAllowed[2] << po.attackAllowed[3];
    return out;
}

/* Tests if the attack chosen is allowed */
bool BattleChoice::match(const BattleChoices &avail) const
{
    if (!avail.attacksAllowed && (attackingChoice() || moveToCenterChoice())) {
        return false;
    }
    if (!avail.switchAllowed && switchChoice()) {
        return false;
    }
    if (rearrangeChoice()) {
        return false;
    }

    if (attackingChoice()) {
        if (avail.struggle() != (attackSlot() == -1))
            return false;
        if (!avail.struggle()) {
            if (attackSlot() < 0 || attackSlot() > 3) {
                //Crash attempt!!
                return false;
            }
            return avail.attackAllowed[attackSlot()];
        }
        return true;
    }

    if (switchChoice()) {
        if (pokeSlot() < 0 || pokeSlot() > 5) {
            //Crash attempt!!
            return false;
        }
        return true;
    }

    if (moveToCenterChoice()) {
        return true;
    }

    //Reached if the type is not known
    return false;
}

QDataStream & operator >> (QDataStream &in, BattleChoice &po)
{
    in >> po.playerSlot >> po.type;

    switch (po.type) {
    case CancelType:
    case DrawType:
    case CenterMoveType:
        break;
    case SwitchType:
        in >> po.choice.switching.pokeSlot;
        break;
    case AttackType:
        in >> po.choice.attack.attackSlot >> po.choice.attack.attackTarget;
        break;
    case RearrangeType:
        for (int i = 0; i < 6; i++) {
            in >> po.choice.rearrange.pokeIndexes[i];
        }
        break;
    }

    return in;
}

QDataStream & operator << (QDataStream &out, const BattleChoice &po)
{
    out << po.playerSlot << po.type;

    switch (po.type) {
    case CancelType:
    case CenterMoveType:
    case DrawType:
        break;
    case SwitchType:
        out << po.choice.switching.pokeSlot;
        break;
    case AttackType:
        out << po.choice.attack.attackSlot << po.choice.attack.attackTarget;
        break;
    case RearrangeType:
        for (int i = 0; i < 6; i++) {
            out << po.choice.rearrange.pokeIndexes[i];
        }
        break;
    }

    return out;
}

QDataStream & operator >> (QDataStream &in, ChallengeInfo & c) {
    in >> c.dsc >> c.opp >> c.clauses >> c.mode;
    return in;
}

QDataStream & operator << (QDataStream &out, const ChallengeInfo & c) {
    out << c.dsc <<  c.opp << c.clauses << c.mode;
    return out;
}

QDataStream & operator >> (QDataStream &in, FindBattleData &f)
{
    quint32 flags;

    in >> flags >> f.range >> f.mode;

    f.rated = flags & 0x01;
    f.sameTier = f.rated || flags & 0x2;
    f.ranged = f.sameTier && flags & 0x4;

    if (f.range < 100)
        f.range = 100;

    return in;
}

QDataStream & operator << (QDataStream &out, const FindBattleData &f)
{
    quint32 flags = 0;

    flags |= f.rated;
    flags |= f.sameTier << 1;
    flags |= f.ranged << 2;

    out << flags << f.range << f.mode;

    return out;
}
