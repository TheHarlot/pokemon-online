#include "player.h"
#include "security.h"
#include "../PokemonInfo/battlestructs.h"

Player::Player(QTcpSocket *sock, int id) : myrelay(sock, id), myid(id)
{
    myip = relay().ip();

    m_state = NotLoggedIn;
    myauth = 0;

    connect(&relay(), SIGNAL(disconnected()), SLOT(disconnected()));
    connect(&relay(), SIGNAL(loggedIn(TeamInfo)), this, SLOT(loggedIn(TeamInfo)));
    connect(&relay(), SIGNAL(messageReceived(QString)), this, SLOT(recvMessage(QString)));
    connect(&relay(), SIGNAL(teamReceived(TeamInfo)), this, SLOT(recvTeam(TeamInfo)));
    connect(&relay(), SIGNAL(challengeStuff(ChallengeInfo)), this, SLOT(challengeStuff(ChallengeInfo)));
    connect(&relay(), SIGNAL(forfeitBattle()), SLOT(battleForfeited()));
    connect(&relay(), SIGNAL(battleMessage(BattleChoice)), SLOT(battleMessage(BattleChoice)));
    connect(&relay(), SIGNAL(battleChat(QString)), SLOT(battleChat(QString)));
    connect(&relay(), SIGNAL(sentHash(QString)), SLOT(hashReceived(QString)));
    connect(&relay(), SIGNAL(wannaRegister()), SLOT(registerRequest()));
    connect(&relay(), SIGNAL(kick(int)), SLOT(playerKick(int)));
    connect(&relay(), SIGNAL(ban(int)), SLOT(playerBan(int)));
    connect(&relay(), SIGNAL(PMsent(int,QString)), this, SLOT(receivePM(int,QString)));
    connect(&relay(), SIGNAL(getUserInfo(QString)), this, SLOT(userInfoAsked(QString)));
    connect(&relay(), SIGNAL(banListRequested()), this, SLOT(giveBanList()));
}

Player::~Player()
{
}

void Player::doWhenDC()
{
    cancelChallenges();
    if (battling())
        battleForfeited();
}

void Player::changeState(int newstate)
{
    m_state = newstate;
}

int Player::auth() const {
    return myauth;
}

void Player::setAuth(int auth)  {
    myauth = auth;
}

void Player::setName(const QString &newname)  {
    team().name = newname;
}

void Player::kick() {
    relay().close();
}

void Player::disconnected()
{
    emit disconnected(id());
}

void Player::battleChat(const QString &s)
{
    if (!isLoggedIn())
        return; //INVALID BEHAVIOR
    emit battleChat(id(), s);
}

void Player::battleMessage(const BattleChoice &b)
{
    if (!isLoggedIn())
        return; //INVALID BEHAVIOR
    emit battleMessage(id(), b);
}

void Player::recvMessage(const QString &mess)
{
    if (!isLoggedIn())
        return; //INVALID BEHAVIOR
    /* for now we just emit the signal, but later we'll do more, such as flood count */
    emit recvMessage(id(), mess);
}

bool Player::challenge(const ChallengeInfo &c)
{
    if (state() != LoggedIn)
	return false;

    relay().sendChallengeStuff(c);

    changeState(Challenged);
    m_challengedby = c;

    return true;
}

ChallengeInfo Player::getChallengeInfo(int id)
{
    if (m_challenged.contains(id)) {
        return m_challenged[id];
    } else {
        return m_challengedby;
    }
}

void Player::battleForfeited()
{
    if (!battling()) {
        return; //INVALID BEHAVIOR
    }

    changeState(LoggedIn);

    emit battleFinished(Forfeit, opponent(), id());
}

void Player::battleResult(int result, int winner, int loser)
{
    relay().sendBattleResult(result, winner, loser);
    if (result == Forfeit || result == Close)
        changeState(LoggedIn);
}

void Player::receivePM(int id, const QString &pm)
{
    if (!isLoggedIn()) {
        //INVALID BEHAVIOR
        return;
    }

    QString str = pm.trimmed();

    if (str.length() == 0) {
        //INVALID BEHAVIOR
        return;
    }

    emit PMReceived(this->id(), id, str);
}

void Player::playerBan(int p) {
    if (!isLoggedIn()) {
        emit info(id(), "Tried to ban while not logged in");
        kick();
        return;
    }

    if (auth() < 2) {
        return; //INVALID BEHAVIOR
    }

    emit playerBan(id(),p);
}

void Player::playerKick(int p) {
    if (!isLoggedIn()) {
        emit info(id(), "Tried to kick while not logged in");
        kick();
        return;
    }

    if (auth() < 1) {
        return; //INVALID BEHAVIOR
    }

    emit playerKick(id(),p);
}

int Player::opponent() const
{
    return m_opponent;
}

void Player::challengeStuff(const ChallengeInfo &c)
{
    int desc = c.desc();
    int id = c;

    if (desc < ChallengeInfo::Sent || desc  >= ChallengeInfo::ChallengeDescLast) {
        // INVALID BEHAVIOR
        return;
    }

    if (!isLoggedIn() || id == this->id()) {
	// INVALID BEHAVIOR
	return;
    }

    if (desc != ChallengeInfo::Sent && desc != ChallengeInfo::Cancelled)
    {
	if (!isChallenged()) {
	    // INVALID BEHAVIOR
	    return;
	}
	if (challengedBy() != id) {
	    // INVALID BEHAVIOR
	    return;
	}
    } else if (desc == ChallengeInfo::Sent) {
	if (battling()) {
	    // INVALID BEHAVIOR
	    return;
	}
    } else  {
        if (battling()) {
            // INVALID BEHAVIOR
            return;
        }
        if (desc == ChallengeInfo::Cancelled) {
            if (!hasChallenged(id)) {
                // INVALID BEHAVIOR
                return;
            }
        }
    }

    if (desc == ChallengeInfo::Sent) {
        addChallenge(c);
    } else if (desc != ChallengeInfo::Accepted) {
	changeState(LoggedIn);
    }

    ChallengeInfo d = c;
    d.opp = this->id();

    emit challengeStuff(d, this->id(), id);
}

void Player::sendChallengeStuff(int stuff, int other)
{
    /* This is either Canceled, Refused, or Busied */
    if (stuff == ChallengeInfo::Cancelled) {
	changeState(LoggedIn);
    } else {
	removeChallenge(other);
    }
    relay().sendChallengeStuff(ChallengeInfo(stuff, other));
}

void Player::startBattle(int id, const TeamBattle &team, const BattleConfiguration &conf)
{
    relay().engageBattle(id, team, conf);

    m_opponent = id;

    changeState(Battling);

    removeChallenge(id);
    cancelChallenges();
}

void Player::cancelChallenges()
{
    foreach(int id, m_challenged)
        emit challengeStuff(ChallengeInfo(ChallengeInfo::Cancelled), this->id(), id);
    m_challenged.clear();
    if (isChallenged()) {
        emit challengeStuff(ChallengeInfo(ChallengeInfo::Busy), this->id(), opponent());
    }
}

void Player::giveBanList()
{
    if (myauth == 0) {
        return; //INVALID BEHAVIOR
    }
    QSet<QString> bannedMembers = SecurityManager::banList();
    foreach(QString s, bannedMembers) {
        relay().notify(NetworkServ::GetBanList, s, SecurityManager::ip(s));
    }
}

bool Player::hasChallenged(int id) const
{
    return m_challenged.contains(id);
}

void Player::addChallenge(const ChallengeInfo &c)
{
    m_challenged.insert(c, c);
}

void Player::removeChallenge(int id)
{
    m_challenged.remove(id);
}

TeamInfo & Player::team()
{
    return myteam;
}

const TeamInfo & Player::team() const
{
    return myteam;
}

Analyzer & Player::relay()
{
    return myrelay;
}

const Analyzer & Player::relay() const
{
    return myrelay;
}

bool Player::battling() const
{
    return state() == Battling;
}

int Player::state() const
{
    return m_state;
}

bool Player::connected() const
{
    return relay().isConnected();
}

bool Player::isChallenged() const
{
    return m_state == Challenged;
}

int Player::challengedBy() const
{
    return m_challengedby;
}

bool Player::isLoggedIn() const
{
    return m_state != NotLoggedIn;
}

int Player::id() const
{
    return myid;
}

BasicInfo Player::basicInfo() const
{
    BasicInfo ret = {team().name, team().info};
    return ret;
}

void Player::loggedIn(const TeamInfo &_team)
{
    if (isLoggedIn())
        return; //INVALID BEHAVIOR


    AuthentificationState st = testAuthentification(_team);

    if (st == Invalid) {
        kick();
        return;
    }

    if (st == Success) {
        team() = _team;
        emit loggedIn(id(), _team.name);
        return;
    }

    /* st == Partial */
    team() = _team;
    return;
}

Player::AuthentificationState Player::testAuthentification(const TeamInfo &team)
{
    if (!SecurityManager::isValid(team.name)) {
        emit info(id(), "invalid name: \"" + team.name + "\"");
        sendMessage("Invalid name. Change your name.");
        return Invalid;
    }

    if (SecurityManager::exist(team.name)) {
        SecurityManager::Member m = SecurityManager::member(team.name);
        if (m.isBanned()) {
            sendMessage("You are banned!");
            return Invalid;
        }
        if (m.isProtected()) {
            relay().notify(NetworkServ::AskForPass, m.salt);
            waiting_name = team.name;
            return Partial;
        }

        myauth = m.authority();

        m.modifyIP(relay().ip());
        m.modifyDate(QDate::currentDate().toString(Qt::ISODate));
        SecurityManager::updateMember(m);
        /* To tell the player he's not registered */
        relay().notify(NetworkServ::Register);
        return Success;
    } else {
        myauth = 0;

        SecurityManager::create(SecurityManager::Member(team.name.toLower(), QDate::currentDate().toString(Qt::ISODate), "000", "", "", relay().ip()));
        /* To tell the player he's not registered */
        relay().notify(NetworkServ::Register);
        return Success;
    }
}

void Player::registerRequest() {
    /* If not logged in or in the middle of an authentification, we quit */
    if (!isLoggedIn() || waiting_name.length() > 0)
        return; //INVALID BEHAVIOR
    SecurityManager::Member m = SecurityManager::member(name());

    if (m.isProtected())
        return; //INVALID BEHAVIOR

    for (int i = 0; i < SecurityManager::Member::saltLength; i++) {
        m.salt[i] = (true_rand() % (122-49)) + 49;
    }

    SecurityManager::updateMemory(m);
    relay().notify(NetworkServ::AskForPass, m.salt);
}

void Player::userInfoAsked(const QString &name)
{
    if (myauth == 0) {
        return; //INVALID BEHAVIOR
    }

    if (!SecurityManager::exist(name)) {
        relay().sendUserInfo(UserInfo(name, UserInfo::NonExistant));
        return;
    }

    SecurityManager::Member m = SecurityManager::member(name);

    UserInfo ret(name, m.isBanned() ? UserInfo::Banned : 0, m.authority(), m.ip.trimmed());
    relay().sendUserInfo(ret);

    QList<QString> aliases = SecurityManager::membersForIp(m.ip.trimmed());

    foreach(QString alias, aliases) {
        relay().notify(NetworkServ::GetUserAlias, alias);
    }
}

void Player::hashReceived(const QString &_hash) {
    QString hash = md5_hash(_hash);
    if (waiting_name.length() > 0) {
        if (hash == SecurityManager::member(waiting_name).hash) {
            SecurityManager::Member m = SecurityManager::member(waiting_name);

            m.modifyIP(relay().ip());
            m.modifyDate(QDate::currentDate().toString(Qt::ISODate));
            m.hash = hash;
            myauth = m.authority();
            SecurityManager::updateMember(m);

            QString temp = waiting_name;
            waiting_name.clear();
            emit loggedIn(id(), temp);
        } else {
            emit info(id(), tr("authentification failed for %1").arg(waiting_name));
            kick();
            return;
        }
    } else {
        SecurityManager::Member m = SecurityManager::member(name());
        if (m.isProtected()) {
            return; //Invalid behavior
        }

        m.hash = hash;
        SecurityManager::updateMember(m);
        emit info(id(), tr("%1 registered.").arg(name()));
    }
}

QString Player::name() const
{
    return team().name;
}

QString Player::ip() const
{
    return myip;
}

void Player::recvTeam(const TeamInfo &team)
{
    cancelChallenges();

    if (team.name == this->team().name) {
        /* No authentification required... */
        this->team() = team;
        emit recvTeam(id(), team.name); // no check needed, going directly there...
        return;
    }

    AuthentificationState s = testAuthentification(team);

    /* just keeping the old name while not logged in */
    QString name = this->team().name;
    this->team() = team;
    this->team().name = name;

    if (s == Success) {
        emit loggedIn(id(), team.name); //checks needed
        return;
    }

    if (s == Invalid) {
        kick();
        return;
    }

    // Partial authentification
    /*
      .
      .
      .
      */
}

void Player::sendMessage(const QString &mess)
{
    relay().sendMessage(mess);
}
