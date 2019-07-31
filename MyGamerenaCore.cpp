#include "MyGamerenaCoreSimple.hpp"
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <unordered_set>
using namespace GameCore;
using namespace std;

inline double Random()
{
	const int RandMax = RAND_MAX + 1;
	return ((double)rand()) / RandMax;
}
inline int Random(int n)
{
	return Random() * n;
}
inline int Random(int l, int r)
{
	if (l >= r) throw InvalidArgumentException("Condition: l < r.");
	return Random(r - l) + l;
}

namespace {
using Stage = int;
using Target = int;
struct StageEnum
{
	constexpr StageEnum() = default;
	const Stage InAction = 1 << 0;
	const Stage BeAttacked = 1 << 1;
	const Stage Waiting = 1 << 2;
};
struct TargetEnum
{
	constexpr TargetEnum() = default;
	const Target Teammate = 1 << 0;
	const Target Enemy = 1 << 1;
	const Target Random = 1 << 2;
};
constexpr StageEnum Stages;
constexpr TargetEnum Targets;
}

struct GamerenaModifier : public EntityAttributeModifier
{
	virtual GamerenaModifier* Clone()const
	{
		return new GamerenaModifier(*this);
	}
	virtual void ModifyState(IState* state)const;
	virtual void Modify(IAttribute* attribute)const;
	int HPModifier = 0;
	int AttackModifier = 0;
	int DefenseModifier = 0;
	int MagicModifier = 0;
	int MagicDefenseModifier = 0;
	int SpeedModifier = 0;
	int AccuracyModifier = 0;
	int IntelligenceModifier = 0;
};

class GamerenaAttribute;

using SkillType = Delegate<void(Entity*, Entity*)>;
struct SkillInfo
{
	SkillType Skill;
	Target TargetType;
	int Priority;
};

class SkillSelector
{
public:
	void AddSkill(const SkillInfo& skill)
	{
		if (!skill.Skill)
			throw InvalidArgumentException("skill is invalid.");
		Skills.push_back(skill);
		TotalPriority += skill.Priority;
	}
	// TODO:这是最简单的技能选择器; 实际将会根据Int实现多种选择器
	SkillInfo& RandomSkill()
	{
		int k = Random(TotalPriority);
		auto iter = Skills.begin();
		while (k >= iter->Priority)
		{
			k -= iter->Priority;
			++iter;
		}
		return *iter;
	}
	void GenerateSkill(GamerenaAttribute* e);
private:
	int TotalPriority = 0;
	List<SkillInfo> Skills;
};

struct GamerenaState : public EntityState
{
	virtual GamerenaState* Clone()const
	{
		return new GamerenaState(*this);
	}
	void GetDamage(int dmg)
	{
		HP = max(HP - dmg, 0);
		if (HP == 0) Active = false;
		for (auto& OnDeathHandler : OnDeath)
			OnDeathHandler(this);
	}
	Stage Stage = Stages.Waiting;
	bool Active = true;
	int NextActionTime = 0;
	int Score = 0;
	int GroupIndex;
	int HP;
	int Attack;
	int Defense;
	int Magic;
	int MagicDefense;
	int Speed;
	int Accuracy;
	int Intelligence;
	List<Delegate<void(GamerenaState*)>> OnDeath;
	//List<Delegate<void(Entity*)>> OnDoAction;
	//List<Delegate<void(Entity*)>> OnDefense;
};

inline GamerenaState* GetGamerenaState(Entity& e)
{
	return dynamic_cast<GamerenaState*>(e.GetState());
}
inline const GamerenaState* GetGamerenaState(const Entity& e)
{
	return dynamic_cast<const GamerenaState*>(e.GetState());
}

struct GamerenaAttribute : public EntityAttribute
{
	virtual GamerenaAttribute* Clone()const
	{
		return new GamerenaAttribute(*this);
	}
	GamerenaAttribute(const string& groupName, const string& name)
	{
		SetName(name);
		OriginGroupIndex = hash<string>()(groupName);
		const size_t RandomF = 419;
		const size_t RandomS = 1284541;
		srand(hash<string>()(name) * RandomF + RandomS);
		BaseHP = Random(200, 350);
		BaseAttack = Random(30, 100);
		BaseDefense = Random(30, 100);
		BaseMagic = Random(30, 100);
		BaseMagicDefense = Random(30, 100);
		BaseSpeed = Random(30, 100);
		BaseAccuracy = Random(30, 100);
		BaseIntelligence = Random(30, 100);
		tSkillSelector.GenerateSkill(this);
	}
	virtual GamerenaState* CreateDefaultState()const;
	SkillSelector tSkillSelector;
	size_t OriginGroupIndex;
	int BaseHP;
	int BaseAttack;
	int BaseDefense;
	int BaseMagic;
	int BaseMagicDefense;
	int BaseSpeed;
	int BaseAccuracy;
	int BaseIntelligence;
};

class UnexceptedCallException : public Exception
{
public:
	UnexceptedCallException(const string& message) : Message(message) {}
	virtual const char* what()noexcept { return Message.c_str(); }
	string Message;
};

class Dispatcher
{
	static bool Compare(Container<Entity> lhs, Container<Entity> rhs)
	{
		return
			GetGamerenaState(*lhs)->NextActionTime
				<
			GetGamerenaState(*rhs)->NextActionTime;
	}
	static int SetNextActionTime(Container<Entity> e)
	{
		auto modifiedAttr = e->GetModifiedAttribute();
		GamerenaAttribute& attr =
			*(GamerenaAttribute*)modifiedAttr.get();
		const int BaseWaitTime = 160;
		return
			BaseWaitTime
			- attr.BaseSpeed * 0.3
			- (attr.BaseSpeed >> 1) * Random();
		// BaseWaitTime(160) - [0.3, 0.8) * Speed[30,100) => WaitTime (80, 151]
	}
public:
	void SetListener(const function<void(Dispatcher*, int)>& listener)
	{
		Listener = listener;
	}
	void AddEntity(Container<Entity> entity)
	{
		SetNextActionTime(entity);
		Entities.push_back(entity);
		push_heap(Entities.begin(), Entities.end(), Compare);
	}
	void DispatchNext()
	{
		GamerenaState* state;
		do
		{
			state = GetGamerenaState(*Entities.front());
			if (!state->Active)
			{
				pop_heap(Entities.begin(), Entities.end(), Compare);
				Entities.pop_back();
			}
		} while (!state->Active&& Entities.size() > 1);
		if (Entities.size() <= 1)
		{
			Listener(this, -1);
			return;
		}
		Time = state->NextActionTime;
		pop_heap(Entities.begin(), Entities.end(), Compare);
		SetNextActionTime(Entities.back());
		Entities.back()->DoActions();
		_LastEntity = Entities.back().get();
		push_heap(Entities.begin(), Entities.end(), Compare);
		if (Listener) Listener(this, Time);
	}
	Entity* LastEntity()
	{
		return _LastEntity;
	}
	int GetCurrentTime()const
	{
		return Time;
	}
private:
	int	Time = 0;
	function<void(Dispatcher*, int)> Listener;
	List<Container<Entity>> Entities;
	Entity* _LastEntity = nullptr;
};

class TargetSelector
{
public:
	// TODO: GetRandomTarget()是最简单的实现; 具体选择算法实现将会取决于Int
	Entity* GetRandomTarget(Entity* entity)
	{
		if (UpdateFlag) Update();
		auto state = GetGamerenaState(*entity);
		if (state->GroupIndex == -1)
		{
			int select = ActiveGroups[Random(ActiveGroups.size())];
			return _LastTarget =
				Entities[select][Random(Entities[select].size())].get();
		}
		int nth =
			find(ActiveGroups.begin(), ActiveGroups.end(), state->GroupIndex)
		  - ActiveGroups.begin();
		int select = Random(ActiveGroups.size() - 1);
		if (select >= nth) ++select;
		select = ActiveGroups[select];
		return _LastTarget = Entities[select][Random(Entities[select].size())].get();
	}
	Entity* GetRandomTeammate(Entity* entity)
	{
		if (UpdateFlag) Update();
		auto state = GetGamerenaState(*entity);
		if (state->GroupIndex == -1)
		{
			int select = ActiveGroups[Random(ActiveGroups.size())];
			return _LastTarget =
				Entities[select][Random(Entities[select].size())].get();
		}
		int teammateCount = Entities[state->GroupIndex].size();
		if (teammateCount > 0)
			return Entities[state->GroupIndex][Random(teammateCount)].get();
		return entity;

	}
	void AddEntity(Container<Entity> entity)
	{
		auto state = GetGamerenaState(*entity);
		auto attrCopy = entity->GetAttributeCopy();
		GamerenaAttribute& attr = *(GamerenaAttribute*)attrCopy.get();
		if (Entities.count(attr.OriginGroupIndex) == 0)
		{
			auto iter =lower_bound(
				ActiveGroups.begin(), ActiveGroups.end(),
				attr.OriginGroupIndex);
			ActiveGroups.insert(iter, attr.OriginGroupIndex);
		}
		Entities[attr.OriginGroupIndex].push_back(entity);
	}
	void SetUpdateFlag(bool flag = true)
	{
		UpdateFlag = flag;
	}
	Entity* LastTarget()
	{
		return _LastTarget;
	}
	int GroupsKeep()
	{
		if (UpdateFlag) Update();
		return ActiveGroups.size();
	}
protected:
	void Update()
	{
		UpdateFlag = false;
		for (auto& pair : Entities)
		{
			List<Container<Entity>>& group = pair.second;
			if (group.size() == 0) continue;
			for (int i = 0; i < group.size();)
			{
				if (!GetGamerenaState(*group[i])->Active)
					group.erase(group.begin() + i);
				else
					++i;
			}
			if (pair.second.size() == 0)
			{
				auto iter = find(ActiveGroups.begin(),
					ActiveGroups.end(), pair.first);
				ActiveGroups.erase(iter);
			}
		}
	}
private:
	bool UpdateFlag = true;
	List<size_t> ActiveGroups;
	HashMap<size_t, List<Container<Entity>>> Entities;
	Entity* _LastTarget;
};

class Game
{
public:
	Game()
	{
		auto listener = [&](Dispatcher* d, int time)
		{
			if (time == -1)
			{
				DispatcherErrorHandler(d);
				return;
			}
		};
		tDispatcher.SetListener(listener);
	}
	void AddName(const string& groupName, const string& name)
	{
		size_t hashCode = hash<string>()(groupName);
		GamerenaAttribute attr(groupName, name);
		attr.AddAction([&](Entity* e)
			{
				auto iattr = e->GetModifiedAttribute();
				GamerenaAttribute& attr =
					*(GamerenaAttribute*)iattr.get();
				SkillInfo skill = attr.tSkillSelector.RandomSkill();
				Entity* target = nullptr;
				switch (skill.TargetType)
				{
				case Targets.Enemy:
					target = tTargetSelector.GetRandomTarget(e);
					break;
				case Targets.Teammate:
					target = tTargetSelector.GetRandomTeammate(e);
					break;
				}
				skill.Skill(e, target);
			});
		auto entity = Container<Entity>(new Entity(&attr, nullptr));
		auto state = GetGamerenaState(*entity);
		state->OnDeath.push_back([&](GamerenaState*){
			tTargetSelector.SetUpdateFlag();
		});
		Groups[hashCode].push_back(entity);
		tDispatcher.AddEntity(entity);
		tTargetSelector.AddEntity(entity);
	}
	void Start()
	{
		while (!IsDone())
			tDispatcher.DispatchNext();
	}
	using Group = List<Container<Entity>>;
	const HashMap<size_t, Group>& GetGroups()const
	{
		return Groups;
	}
	bool IsDone()
	{
		if (DoneFlag)
			return true;
		if (tTargetSelector.GroupsKeep() < 2)
			return DoneFlag = true;
		return false;
	}
protected:
	void DispatcherErrorHandler(Dispatcher* d)
	{
		//TODO
		DoneFlag = true;
	}
private:
	bool DoneFlag = false;
	Dispatcher tDispatcher;
	TargetSelector tTargetSelector;
	HashMap<size_t, Group> Groups;
};

void ShowObject(const Entity& e, int space, int level);

void CausePhysicDamage(Entity* p, Entity* t, double dmgFactor = 1.0)
{
	GamerenaState& pState = *GetGamerenaState(*p);
	GamerenaState& tState = *GetGamerenaState(*t);
	auto piAttr = p->GetModifiedAttribute();
	auto tiAttr = t->GetModifiedAttribute();
	GamerenaAttribute& pAttr = *(GamerenaAttribute*)piAttr.get();
	GamerenaAttribute& tAttr = *(GamerenaAttribute*)tiAttr.get();
	// 闪避判定
	const int BaseDodgeChance = 16;
	int dodgeChance = BaseDodgeChance
		+ (tAttr.BaseAccuracy - pAttr.BaseAccuracy) / 4
		+ (tAttr.BaseDefense - pAttr.BaseAttack) / 8;
	if (Random(100) < dodgeChance)
	{
		cout << " 但 " << tAttr.GetName() << " 闪避了攻击.\n";
		return;
	}
	const int BaseDamage = 15;
	int damage = max(1,
		(int)(BaseDamage
			+ pAttr.BaseAttack * 0.3 + pAttr.BaseAttack * 0.9 * Random()
			- tAttr.BaseDefense * 0.2 + tAttr.BaseDefense * 1.3 * Random()));
	cout << " 对 " << tAttr.GetName() << " 造成了 " << damage << "点伤害.\n";
	pState.Score += damage;
	tState.GetDamage(damage);
	ShowObject(*t, 4, 0);
	if (tState.Active == false)
	{
		pState.Score += 30;
		cout << tAttr.GetName() << " 死亡了, 凶手是 " << pAttr.GetName() << '\n';
	}
}

void CauseMagicDamage(Entity* p, Entity* t, double dmgFactor = 1.0)
{
	GamerenaState& pState = *GetGamerenaState(*p);
	GamerenaState& tState = *GetGamerenaState(*t);
	auto piAttr = p->GetModifiedAttribute();
	auto tiAttr = t->GetModifiedAttribute();
	GamerenaAttribute& pAttr = *(GamerenaAttribute*)piAttr.get();
	GamerenaAttribute& tAttr = *(GamerenaAttribute*)tiAttr.get();
	// 闪避判定
	const int BaseDodgeChance = 25;
	int dodgeChance = BaseDodgeChance
		- pAttr.BaseIntelligence >> 3
		+ (tAttr.BaseAccuracy - pAttr.BaseAccuracy) / 8
		+ (tAttr.BaseMagicDefense - pAttr.BaseMagic) / 8;
	if (Random(100) < dodgeChance)
	{
		cout << " 但 " << tAttr.GetName() << " 闪避了攻击.\n";
		return;
	}
	const int BaseDamage = 25;
	int damage = max(1,
		(int)(BaseDamage
			+ pAttr.BaseMagic * 0.6 + pAttr.BaseMagic * 0.6 * Random()
			- tAttr.BaseMagicDefense * 0.75 + tAttr.BaseMagicDefense * 0.75 * Random()
			+ pAttr.BaseIntelligence * 0.2));
	cout << " 对 " << tAttr.GetName() << " 造成了 " << damage << "点魔法伤害.\n";
	pState.Score += damage;
	tState.GetDamage(damage);
	ShowObject(*t, 4, 0);
	if (tState.Active == false)
	{
		pState.Score += 30;
		cout << tAttr.GetName() << " 死亡了, 凶手是 " << pAttr.GetName() << '\n';
	}
}

void MakeCuel(Entity* p, Entity* t, double hFactor = 1.0)
{
	GamerenaState& pState = *GetGamerenaState(*p);
	GamerenaState& tState = *GetGamerenaState(*t);
	auto piAttr = p->GetModifiedAttribute();
	auto tiAttr = t->GetModifiedAttribute();
	GamerenaAttribute& pAttr = *(GamerenaAttribute*)piAttr.get();
	GamerenaAttribute& tAttr = *(GamerenaAttribute*)tiAttr.get();
	const int BaseHeal = 10;
	int heal = max(1,
		(int)(BaseHeal
			+ pAttr.BaseMagic * 0.25 + pAttr.BaseMagic * 0.35 * Random()
			+ pAttr.BaseIntelligence * 0.4));
	heal = min(tAttr.BaseHP - tState.HP, heal);
	pState.Score += heal;
	tState.HP += heal;
	cout << " " << tAttr.GetName() << " 恢复了 "<< heal << " 点生命值.\n";
	ShowObject(*t, 4, 0);
}

void BaseAttack(Entity* p, Entity* t)
{
	ShowObject(*p, 0, 0);
	cout << "  发起了攻击,";
	CausePhysicDamage(p, t);
}

void BaseMagic(Entity * p, Entity * t)
{
	ShowObject(*p, 0, 0);
	cout << "  使用法术攻击,";
	CauseMagicDamage(p, t);
}

void FireBall(Entity* p, Entity* t)
{
	ShowObject(*p, 0, 0);
	cout << "  发射出火球,";
	CauseMagicDamage(p, t, 1.8);
}

void Critical(Entity* p, Entity* t)
{
	ShowObject(*p, 0, 0);
	cout << "  瞄准了目标的弱点攻击,";
	CausePhysicDamage(p, t, 2.15);
}

void Cuel(Entity* p, Entity* t)
{
	ShowObject(*p, 0, 0);
	cout << "  使用了治愈魔法,";
	MakeCuel(p, t, 1.2);
}


void SkillSelector::GenerateSkill(GamerenaAttribute* pAttr)
{
	GamerenaAttribute& attr = *pAttr;
	int BaseAttackPriority =
		250 + (attr.BaseAttack - attr.BaseMagic) * 4 * (0.5 + Random());
	int BaseMagicPriority =
		250 + (attr.BaseMagic - attr.BaseAttack) * 4 * (0.5 + Random());
	int FireBallPriority =
		50 + (attr.BaseIntelligence >> 1) + (attr.BaseMagic);
	int CriticalPriority =
		30 + (attr.BaseIntelligence >> 2) + (attr.BaseAttack >> 1)
		+ (attr.BaseAccuracy >> 1);
	int CuelPriority =
		60 + (attr.BaseIntelligence >> 1) + (attr.BaseMagic >> 2);
	AddSkill({ BaseAttack, Targets.Enemy, BaseAttackPriority });
	AddSkill({ BaseMagic, Targets.Enemy, BaseMagicPriority });
	if (FireBallPriority > 140)
		AddSkill({ FireBall, Targets.Enemy, FireBallPriority });
	if (CriticalPriority > 125)
		AddSkill({ Critical, Targets.Enemy, CriticalPriority });
	if (CuelPriority > 100)
		AddSkill({ Cuel, Targets.Teammate, CuelPriority });
}

inline void GamerenaModifier::ModifyState(IState* state)const
{
	GamerenaState* pState = dynamic_cast<GamerenaState*>(state);
	if (pState == nullptr)
		throw InvalidArgumentException(
			"state can\'t be null and have type of \"EntityState\".");
	GamerenaState& State = *pState;
	State.HP = max(State.HP + HPModifier, 0);
}

inline void GamerenaModifier::Modify(IAttribute* attribute)const
{
	GamerenaAttribute* pAttribute =
		dynamic_cast<GamerenaAttribute*>(attribute);
	if (pAttribute == nullptr)
		throw InvalidArgumentException(
			"attribute can\'t be null and have type of \"EntityAttribute\".");
	GamerenaAttribute& Attribute = *pAttribute;
	Attribute.BaseHP =
		max(Attribute.BaseHP + HPModifier, 1);
	Attribute.BaseAttack =
		max(Attribute.BaseAttack + AttackModifier, 0);
	Attribute.BaseDefense =
		Attribute.BaseDefense + DefenseModifier;
	Attribute.BaseMagic =
		max(Attribute.BaseMagic + MagicModifier, 0);
	Attribute.BaseMagicDefense =
		Attribute.BaseMagicDefense + MagicDefenseModifier;
	Attribute.BaseSpeed =
		Attribute.BaseSpeed + SpeedModifier;
	Attribute.BaseAccuracy =
		max(Attribute.BaseSpeed + AccuracyModifier, 5);
	Attribute.BaseIntelligence =
		max(Attribute.BaseIntelligence + IntelligenceModifier, 0);
}

inline void ResetState(GamerenaState& state, const GamerenaAttribute& attr)
{
	state.GroupIndex = attr.OriginGroupIndex;
	state.HP = attr.BaseHP;
	state.Attack = attr.BaseAttack;
	state.Defense = attr.BaseDefense;
	state.Magic = attr.BaseMagic;
	state.MagicDefense = attr.BaseMagicDefense;
	state.Speed = attr.BaseSpeed;
	state.Accuracy = attr.BaseAccuracy;
	state.Intelligence = attr.BaseIntelligence;
}

inline GamerenaState* GamerenaAttribute::CreateDefaultState()const
{
	auto state = new GamerenaState();
	ResetState(*state, *this);
	return state;
}

void ShowObject(const Entity& e, int space, int level)
{
	const GamerenaState& state = *GetGamerenaState(e);
	auto modifiedAttr = e.GetModifiedAttribute();
	GamerenaAttribute& attr = *(GamerenaAttribute*)modifiedAttr.get();
	auto PrintSpace = [&](){
		for (int i = 0; i < space; ++i) cout.put('\0');
	};
	PrintSpace();
	cout << "Name: " << attr.GetName() << "  "
		 << "HP: " << state.HP << " / " << attr.BaseHP << "  <";
	int b = (state.HP + 10) / 20;
	for (int i = 0; i < b; ++i) cout.put(2);
	for (int i = b; i < (attr.BaseHP + 10) / 20; ++i) cout.put(1);
	cout << ">\n";
	if (level > 1)
	{
		PrintSpace();
		cout << "Score: " << state.Score << '\n';
	}
	if (!state.Active)
	{
		PrintSpace();
		cout << "+-| xD\n";
		return;
	}
	if (level > 0)
	{
		PrintSpace();
		cout << "Atk: " << attr.BaseAttack << "\tDef: " << attr.BaseDefense
			 << "\t\tAcc: " << attr.BaseAccuracy << '\n';
		PrintSpace();
		cout << "Mag: " << attr.BaseMagic << "\tMagDef: " << attr.BaseMagicDefense
			 << "\tSpd: " << attr.BaseSpeed << "\tInt: " << attr.BaseIntelligence
			 << '\n';
	}
}

int main()
{
	ios::sync_with_stdio(false);
	string fullName;
	const string DefaultSeed = "${DefaultSeed}";
	string seed = DefaultSeed;
	Game game;
	unordered_set<string> nameUsed;
	while (getline(cin, fullName))
	{
		if (fullName[0] == '>')
		{
			// TODO: CommandMode
			cout << "Command is working.\n";
			continue;
		}
		size_t nameLength = fullName.find_last_of('@');
		string name(fullName, 0, nameLength);
		if (name == "")
		{
			cout << "Name shouldn\'t be empty.\n";
			continue;
		}
		if (nameUsed.count(name) == 0)
		{
			nameUsed.insert(name);
			string groupName;
			if (nameLength != string::npos)
				groupName = string(fullName, nameLength + 1, string::npos);
			else
				groupName = "~@Default";
			if (groupName == "")
			{
				cout << "GroupName shouldn\'t be empty.\n";
				continue;
			}
			cout << "Name: " << name << ", GroupName: " << groupName << ".\n";
			game.AddName(groupName, name);
		}
		else
		{
			cout << "Name \"" << name << "\" has been used.\n"
				 << "Please use another name instead.\n";
		}
	}
	const size_t srandF = 73;
	const size_t srandS = 749431;
	srand(/*hash<string>()(seed)*/time(0) * srandF + srandS);
	for (auto& pair : game.GetGroups())
	{
		cout << "GroupName: " << pair.first << '\n';
		for (auto& member : pair.second)
		{
			ShowObject(*member, 4, 1);
			cout.put('\n');
		}
	}
	cin.ignore(1024, '\n');
	cout << "PressAnyKeyToStart...\n";
	cin.get();
	game.Start();
	cin.ignore(1024, '\n');
	for (auto& pair : game.GetGroups())
	{
		cout << "GroupName: " << pair.first << '\n';
		for (auto& member : pair.second)
		{
			ShowObject(*member, 4, 2);
			cout.put('\n');
		}
	}
	cout << "Done...\n";
	cin.get();
}
