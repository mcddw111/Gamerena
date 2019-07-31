#ifndef _MYGAMERENA_CORE_
#define _MYGAMERENA_CORE_

#include <vector>
#include <unordered_map>
#include <memory>
#include <utility>
#include <string>
#include <algorithm>
#include <functional>
#include <exception>

namespace GameCore
{

template<typename ValueType>
using Container = std::shared_ptr<ValueType>;
template<typename ValueType>
using List = std::vector<ValueType>;
template<typename FunctionType>
using Delegate = std::function<FunctionType>;
template<typename KeyType, typename ValueType>
using HashMap = std::unordered_map<KeyType, ValueType>;
using std::string;
using std::move;
using std::max;
using std::remove;
using Exception = std::exception;

class NullArgumentException : public Exception
{
public:
	NullArgumentException(const string& message) : Message(message) {}
	virtual const char* what()noexcept { return Message.c_str(); }
	string Message;
};

class InvalidArgumentException : public Exception
{
public:
	InvalidArgumentException(const string& message) : Message(message) {}
	virtual const char* what()noexcept { return Message.c_str(); }
	string Message;
};

struct INamable
{
public:
	INamable() = default;
	virtual ~INamable() = default;
	virtual bool HasName(const string& name) { return name == Name; }
	const string& GetName()const { return Name; }
protected:
	void SetName(const string& name)
	{
		Name = name;
	}
private:
	string Name;
};

struct ICloneable
{
	ICloneable() = default;
	virtual ~ICloneable() = default;
	virtual ICloneable* Clone()const = 0;
};

struct IState : public ICloneable
{
	IState() = default;
	virtual ~IState() = default;
	virtual IState* Clone()const = 0;
};

struct IAttribute : public ICloneable, public INamable
{
	IAttribute() = default;
	virtual ~IAttribute() = default;
	virtual IAttribute* Clone()const = 0;
	virtual IState* CreateDefaultState()const = 0;
};
using Type = IAttribute;


class GameObject : public ICloneable, public INamable
{
public:
	GameObject() = default;
	GameObject(const GameObject& other) :
		State(other.State->Clone()),
		Attribute(other.Attribute)
	{
		SetName(Attribute->GetName());
	}
	GameObject(GameObject&& other)noexcept :
		State(other.State),
		Attribute(other.Attribute)
	{
		SetName(Attribute->GetName());
	}
	GameObject& operator=(const GameObject& other)
	{
		State = Container<IState>(other.State->Clone());
		Attribute = other.Attribute;
		SetName(Attribute->GetName());
		return *this;
	}
	GameObject& operator=(GameObject&& other)noexcept
	{
		State = other.State;
		Attribute = other.Attribute;
		SetName(Attribute->GetName());
		return *this;
	}
	virtual ~GameObject() = default;

	virtual GameObject* Clone()const
	{
		return new GameObject(*this);
	}
	bool IsTypeEquals(const GameObject& other)const
	{
		return Attribute == other.Attribute;
	}
	bool HasType(IAttribute* attribute)const
	{
		return Attribute.get() == attribute;
	}
	Container<IState> GetStateCopy()const
	{
		return Container<IState>(State->Clone());
	}
	Container<IAttribute> GetAttributeCopy()const
	{
		return Container<IAttribute>(Attribute->Clone());
	}
	const IState* GetState()const { return State.get(); }
	IState* GetState() { return State.get(); }
protected:
	const IAttribute* GetAttribute()const { return Attribute.get(); }
	IAttribute* GetAttribute() { return Attribute.get(); }
	void SetState(IState* state)
	{
		State = Container<IState>(state->Clone());
	}
	template<typename StateType>
	void SetState(Container<StateType> state)
	{
		State = Container<IState>(state->Clone());
	}
	void SetAttribute(IAttribute* attribute)
	{
		Attribute = Container<IAttribute>(attribute->Clone());
	}
	template<typename AttributeType>
	void SetAttribute(Container<AttributeType> attribute)
	{
		Attribute = Container<IAttribute>(attribute);
	}
private:
	Container<IState> State = nullptr;
	Container<IAttribute> Attribute = nullptr;
};

struct IModifier : public ICloneable, public INamable
{
	IModifier() = default;
	virtual ~IModifier() = default;
	virtual IModifier* Clone()const = 0;
	virtual void Modify(IAttribute*)const = 0;
	int RoundCount = 0;
	int TimeCount = 0;
	int MaxRound = -1;
	int MaxTime = -1;
};

class StateBase : public IState
{
public:
	StateBase() = default;
	StateBase(const StateBase& other) : Modifiers(other.Modifiers.size())
	{
		for (size_t i = 0; i < other.Modifiers.size(); ++i)
			Modifiers[i] = Container<IModifier>(other.Modifiers[i]->Clone());
	}
	StateBase(StateBase&& other) = default;
	StateBase& operator=(const StateBase& other)
	{
		Modifiers.resize(other.Modifiers.size());
		for (size_t i = 0; i < other.Modifiers.size(); ++i)
			Modifiers[i] = Container<IModifier>(other.Modifiers[i]->Clone());
		return *this;
	}
	StateBase& operator=(StateBase&& other) = default;
	virtual ~StateBase() = default;
	virtual StateBase* Clone()const { return new StateBase(*this); }
	void RemoveModifier(const string& name)
	{
		auto iter = remove_if(Modifiers.begin(), Modifiers.end(),
			[&](Container<IModifier> modifier)
			{ return modifier->HasName(name); });
		Modifiers.erase(iter, Modifiers.end());
	}
protected:
	IAttribute* GetModifiedAttribute(const IAttribute* attribute)const
	{ // Tips: You need release the resource of the return pointer
		auto result = attribute->Clone();
		for (auto& modifier : Modifiers)
			modifier->Modify(result);
		return result;
	}
	void AddModifier(const IModifier* modifier)
	{
		Modifiers.push_back(Container<IModifier>(modifier->Clone()));
	}
private:
	List<Container<IModifier>> Modifiers;
};

struct EntityAttribute;
struct EntityState;
class Entity;

struct EntityAttributeModifier : public IModifier
{
	virtual EntityAttributeModifier* Clone()const
	{
		return new EntityAttributeModifier(*this);
	}
	virtual ~EntityAttributeModifier() = default;
	virtual void ModifyState(IState* state)const;
	virtual void Modify(IAttribute* attribute)const;
};

struct EntityState : public StateBase
{
	EntityState() = default;
	virtual EntityState* Clone()const { return new EntityState(*this); }
	virtual ~EntityState() = default;
	Container<EntityAttribute>
	GetModifiedAttribute(const EntityAttribute* attribute)const;
	void AddModifier(const EntityAttributeModifier* modifier)
	{
		if (modifier == nullptr)
			throw NullArgumentException("modifier can\'t be null.");
		modifier->ModifyState(this);
		StateBase::AddModifier((const IModifier*)modifier);
	}
};

struct EntityAttribute : public IAttribute
{
	virtual EntityAttribute* Clone()const
	{
		return new EntityAttribute(*this);
	}
	virtual ~EntityAttribute() = default;
	virtual EntityState* CreateDefaultState()const
	{
		return new EntityState();
	}
	using ActionHandler = Delegate<void(Entity*)>;
	void AddAction(const ActionHandler& handler)
	{
		if (!handler)
			throw InvalidArgumentException("handler is null/invalid.");
		Actions.push_back(handler);
	}
	void AddNamedAction(const ActionHandler& handler, const string& name)
	{
		AddAction(handler);
		NamedActions[name] = handler;
	}
	bool TryInvokeAction(const string& actionName, Entity* entity)
	{
		if (NamedActions.count(actionName))
		{
			NamedActions[actionName](entity);
			return true;
		}
		return false;
	}
	void InvokeAllActions(Entity* entity)
	{
		for (auto& action : Actions)
			action(entity);
	}
private:
	List<ActionHandler> Actions;
	HashMap<string, ActionHandler> NamedActions;
};

inline Container<EntityAttribute>
EntityState::GetModifiedAttribute(const EntityAttribute* attribute)const
{
	auto result =
		(EntityAttribute*)StateBase::GetModifiedAttribute(attribute);
	return Container<EntityAttribute>(result);
}

inline void EntityAttributeModifier::ModifyState(IState* state)const
{
	EntityState* pEntityState = dynamic_cast<EntityState*>(state);
	if (pEntityState == nullptr)
		throw InvalidArgumentException(
			"state can\'t be null and have type of \"EntityState\".");
	// TODO
}

inline void EntityAttributeModifier::Modify(IAttribute* attribute)const
{
	EntityAttribute* pEntityAttribute =
		dynamic_cast<EntityAttribute*>(attribute);
	if (pEntityAttribute == nullptr)
		throw InvalidArgumentException(
			"attribute can\'t be null and have type of \"EntityAttribute\".");
	// TODO
}

class Entity : public GameObject
{
	static HashMap<string, Container<EntityAttribute>> AttributeMap;
	static HashMap<string, Container<EntityState>> StateMap;
public:
	static bool NamedAttribute(EntityAttribute* attribute, const string& name,
		bool replaceOld = false)
	{
		if (attribute == nullptr)
			throw NullArgumentException("attribute can\'t be null.");
		if (AttributeMap.count(name) && !replaceOld)
			throw InvalidArgumentException("name has been used.");
		AttributeMap[name] = Container<EntityAttribute>(attribute->Clone());
	}
	static bool NamedState(EntityState* state, const string& name,
		bool replaceOld = false)
	{
		if (state == nullptr)
			throw NullArgumentException("state can\'t be null.");
		if (StateMap.count(name) && !replaceOld)
			throw InvalidArgumentException("name has been used.");
		StateMap[name] = Container<EntityState>(state->Clone());
	}
	Entity(EntityAttribute* attribute, EntityState* state) :
		Entity(Container<EntityAttribute>
			(attribute ? attribute->Clone() : nullptr), state) {}
	Entity(EntityAttribute* attribute, const string& stateName) :
		Entity(Container<EntityAttribute>
			(attribute ? attribute->Clone() : nullptr),
			 StateMap[stateName].get()) {}
	Entity(const string& attributeName, EntityState* state) :
		Entity(AttributeMap[attributeName], state) {}
	Entity(const string& attributeName, const string& stateName) :
		Entity(AttributeMap[attributeName], StateMap[stateName].get()) {}
	virtual Entity* Clone()const { return new Entity(*this); }
	virtual ~Entity() = default;
	Container<EntityAttribute> GetModifiedAttribute()const
	{
		return ((EntityState*)GetState())
			->GetModifiedAttribute((EntityAttribute*)GetAttribute());
	}
	void AddModifier(EntityAttributeModifier* modifier)
	{
		((EntityState*)GetState())->AddModifier(modifier);
	}
	void RemoveModifier(const string& name)
	{
		((EntityState*)GetState())->RemoveModifier(name);
	}
	void DoActions()
	{
		((EntityAttribute*)GetAttribute())->InvokeAllActions(this);
	}
	bool TryDoAction(const string& actionName)
	{
		return ((EntityAttribute*)GetAttribute())->TryInvokeAction(actionName, this);
	}
protected:
	Entity(Container<EntityAttribute> attribute, EntityState* state)
	{
		if (attribute == nullptr)
			throw NullArgumentException("attribute can\'t be null.");
		SetAttribute(attribute);
		if (state == nullptr)
		{
			state = attribute->CreateDefaultState();
			SetState(state);
			delete state;
		}
		else
		{
			SetState(state);
		}
	}
};
HashMap<string, Container<EntityAttribute>> Entity::AttributeMap;
HashMap<string, Container<EntityState>> Entity::StateMap;

} // namespace GameCore

#endif