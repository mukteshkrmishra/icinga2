/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012 Icinga Development Team (http://www.icinga.org/)        *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "icinga/service.h"
#include "icinga/checkcommand.h"
#include "icinga/icingaapplication.h"
#include "icinga/checkresultmessage.h"
#include "icinga/cib.h"
#include "remoting/endpointmanager.h"
#include "base/dynamictype.h"
#include "base/objectlock.h"
#include "base/logger_fwd.h"
#include <boost/smart_ptr/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/exception/diagnostic_information.hpp>

using namespace icinga;

const int Service::DefaultMaxCheckAttempts = 3;
const double Service::DefaultCheckInterval = 5 * 60;
const double Service::CheckIntervalDivisor = 5.0;

boost::signals2::signal<void (const Service::Ptr&)> Service::OnCheckerChanged;
boost::signals2::signal<void (const Service::Ptr&)> Service::OnNextCheckChanged;

CheckCommand::Ptr Service::GetCheckCommand(void) const
{
	return CheckCommand::GetByName(m_CheckCommand);
}

long Service::GetMaxCheckAttempts(void) const
{
	if (m_MaxCheckAttempts.IsEmpty())
		return DefaultMaxCheckAttempts;

	return m_MaxCheckAttempts;
}

TimePeriod::Ptr Service::GetCheckPeriod(void) const
{
	return TimePeriod::GetByName(m_CheckPeriod);
}

double Service::GetCheckInterval(void) const
{
	if (m_CheckInterval.IsEmpty())
		return DefaultCheckInterval;

	return m_CheckInterval;
}

double Service::GetRetryInterval(void) const
{
	if (m_RetryInterval.IsEmpty())
		return GetCheckInterval() / CheckIntervalDivisor;

	return m_RetryInterval;
}

Array::Ptr Service::GetCheckers(void) const
{
	return m_Checkers;
}

void Service::SetSchedulingOffset(long offset)
{
	m_SchedulingOffset = offset;
}

long Service::GetSchedulingOffset(void)
{
	return m_SchedulingOffset;
}

void Service::SetNextCheck(double nextCheck)
{
	m_NextCheck = nextCheck;
	Touch("next_check");
}

double Service::GetNextCheck(void)
{
	return m_NextCheck;
}

void Service::UpdateNextCheck(void)
{
	ObjectLock olock(this);

	double interval;

	if (GetStateType() == StateTypeSoft)
		interval = GetRetryInterval();
	else
		interval = GetCheckInterval();

	double now = Utility::GetTime();
	double adj = 0;

	if (interval > 1)
		adj = fmod(now * 100 + GetSchedulingOffset(), interval * 100) / 100.0;

	SetNextCheck(now - adj + interval);
}

void Service::SetCurrentChecker(const String& checker)
{
	m_CurrentChecker = checker;
	Touch("current_checker");
}

String Service::GetCurrentChecker(void) const
{
	return m_CurrentChecker;
}

void Service::SetCurrentCheckAttempt(long attempt)
{
	m_CheckAttempt = attempt;
	Touch("check_attempt");
}

long Service::GetCurrentCheckAttempt(void) const
{
	if (m_CheckAttempt.IsEmpty())
		return 1;

	return m_CheckAttempt;
}

void Service::SetState(ServiceState state)
{
	m_State = static_cast<long>(state);
	Touch("state");
}

ServiceState Service::GetState(void) const
{
	if (m_State.IsEmpty())
		return StateUnknown;

	int ivalue = static_cast<int>(m_State);
	return static_cast<ServiceState>(ivalue);
}

void Service::SetLastState(ServiceState state)
{
	m_LastState = static_cast<long>(state);

	Touch("last_state");
}

ServiceState Service::GetLastState(void) const
{
	if (m_LastState.IsEmpty())
		return StateUnknown;

	int ivalue = static_cast<int>(m_LastState);
	return static_cast<ServiceState>(ivalue);
}

void Service::SetStateType(StateType type)
{
	m_StateType = static_cast<long>(type);
	Touch("state_type");
}

StateType Service::GetStateType(void) const
{
	if (m_StateType.IsEmpty())
		return StateTypeSoft;

	int ivalue = static_cast<int>(m_StateType);
	return static_cast<StateType>(ivalue);
}

void Service::SetLastStateType(StateType type)
{
	m_LastStateType = static_cast<long>(type);
	Touch("last_state_type");
}

StateType Service::GetLastStateType(void) const
{
	if (m_LastStateType.IsEmpty())
		return StateTypeSoft;

	int ivalue = static_cast<int>(m_LastStateType);
	return static_cast<StateType>(ivalue);
}

void Service::SetLastReachable(bool reachable)
{
	m_LastReachable = reachable;
	Touch("last_reachable");
}

bool Service::GetLastReachable(void) const
{
	if (m_LastReachable.IsEmpty())
		return true;

	return m_LastReachable;
}

void Service::SetLastCheckResult(const Dictionary::Ptr& result)
{
	m_LastResult = result;
	Touch("last_result");
}

Dictionary::Ptr Service::GetLastCheckResult(void) const
{
	return m_LastResult;
}

void Service::SetLastStateChange(double ts)
{
	m_LastStateChange = ts;
	Touch("last_state_change");
}

double Service::GetLastStateChange(void) const
{
	if (m_LastStateChange.IsEmpty())
		return IcingaApplication::GetInstance()->GetStartTime();

	return m_LastStateChange;
}

void Service::SetLastHardStateChange(double ts)
{
	m_LastHardStateChange = ts;
	Touch("last_hard_state_change");
}

double Service::GetLastHardStateChange(void) const
{
	if (m_LastHardStateChange.IsEmpty())
		return IcingaApplication::GetInstance()->GetStartTime();

	return m_LastHardStateChange;
}

bool Service::GetEnableActiveChecks(void) const
{
	if (m_EnableActiveChecks.IsEmpty())
		return true;
	else
		return m_EnableActiveChecks;
}

void Service::SetEnableActiveChecks(bool enabled)
{
	m_EnableActiveChecks = enabled ? 1 : 0;
	Touch("enable_active_checks");
}

bool Service::GetEnablePassiveChecks(void) const
{
	if (m_EnablePassiveChecks.IsEmpty())
		return true;
	else
		return m_EnablePassiveChecks;
}

void Service::SetEnablePassiveChecks(bool enabled)
{
	m_EnablePassiveChecks = enabled ? 1 : 0;
	Touch("enable_passive_checks");
}

bool Service::GetForceNextCheck(void) const
{
	if (m_ForceNextCheck.IsEmpty())
		return false;

	return static_cast<bool>(m_ForceNextCheck);
}

void Service::SetForceNextCheck(bool forced)
{
	m_ForceNextCheck = forced ? 1 : 0;
	Touch("force_next_check");
}

void Service::ProcessCheckResult(const Dictionary::Ptr& cr)
{
	double now = Utility::GetTime();

	if (!cr->Contains("schedule_start"))
		cr->Set("schedule_start", now);

	if (!cr->Contains("schedule_end"))
		cr->Set("schedule_end", now);

	if (!cr->Contains("execution_start"))
		cr->Set("execution_start", now);

	if (!cr->Contains("execution_end"))
		cr->Set("execution_end", now);

	bool reachable = IsReachable();

	Host::Ptr host = GetHost();
	bool host_reachable = true;

	if (host)
		host_reachable = host->IsReachable();

	ASSERT(!OwnsLock());
	ObjectLock olock(this);

	Dictionary::Ptr old_cr = GetLastCheckResult();
	ServiceState old_state = GetState();
	StateType old_stateType = GetStateType();
	long old_attempt = GetCurrentCheckAttempt();
	bool recovery;

	/* The ExecuteCheck function already sets the old state, but we need to do it again
	 * in case this was a passive check result. */
	SetLastState(old_state);
	SetLastStateType(old_stateType);
	SetLastReachable(reachable);

	long attempt;

	if (cr->Get("state") == StateOK) {
		if (old_state == StateOK && old_stateType == StateTypeSoft)
			SetStateType(StateTypeHard); // SOFT OK -> HARD OK

		attempt = 1;
		recovery = true;
	} else {
		if (old_attempt >= GetMaxCheckAttempts()) {
			SetStateType(StateTypeHard);
			attempt = 1;
		} else if (GetStateType() == StateTypeSoft || GetState() == StateOK) {
			SetStateType(StateTypeSoft);
			attempt = old_attempt + 1;
		} else {
			attempt = old_attempt;
		}

		recovery = false;
	}

	SetCurrentCheckAttempt(attempt);

	int state = cr->Get("state");
	SetState(static_cast<ServiceState>(state));

	bool call_eventhandler = false;

	if (old_state != GetState()) {
		SetLastStateChange(now);

		/* remove acknowledgements */
		if (GetAcknowledgement() == AcknowledgementNormal ||
		    (GetAcknowledgement() == AcknowledgementSticky && GetStateType() == StateTypeHard && GetState() == StateOK)) {
			SetAcknowledgement(AcknowledgementNone);
			SetAcknowledgementExpiry(0);
		}

		/* reschedule service dependencies */
		BOOST_FOREACH(const Service::Ptr& parent, GetParentServices()) {
			ObjectLock olock(parent);
			parent->SetNextCheck(Utility::GetTime());
		}

		/* reschedule host dependencies */
		BOOST_FOREACH(const Host::Ptr& parent, GetParentHosts()) {
			Service::Ptr service = parent->GetHostCheckService();

			if (service && service->GetName() != GetName()) {
				ObjectLock olock(service);
				service->SetNextCheck(Utility::GetTime());
			}
		}

		call_eventhandler = true;
	}

	bool hardChange = (GetStateType() == StateTypeHard && old_stateType == StateTypeSoft);

	if (old_state != GetState() && old_stateType == StateTypeHard && GetStateType() == StateTypeHard)
		hardChange = true;

	if (hardChange)
		SetLastHardStateChange(now);

	if (GetState() != StateOK)
		TriggerDowntimes();

	Service::UpdateStatistics(cr);

	bool in_downtime = IsInDowntime();
	bool send_notification = hardChange && reachable && !in_downtime && !IsAcknowledged();

	if (old_state == StateOK && old_stateType == StateTypeSoft)
		send_notification = false; /* Don't send notifications for SOFT-OK -> HARD-OK. */

	bool send_downtime_notification = m_LastInDowntime != in_downtime;
	m_LastInDowntime = in_downtime;
	Touch("last_in_downtime");

	olock.Unlock();

	Dictionary::Ptr vars_after = boost::make_shared<Dictionary>();
	vars_after->Set("state", GetState());
	vars_after->Set("state_type", GetStateType());
	vars_after->Set("attempt", GetCurrentCheckAttempt());
	vars_after->Set("reachable", reachable);
	vars_after->Set("host_reachable", host_reachable);

	if (old_cr)
		cr->Set("vars_before", old_cr->Get("vars_after"));

	cr->Set("vars_after", vars_after);

	cr->Seal();

	olock.Lock();
	SetLastCheckResult(cr);
	olock.Unlock();

	/* Flush the object so other instances see the service's
	 * new state when they receive the CheckResult message */
	Flush();

	RequestMessage rm;
	rm.SetMethod("checker::CheckResult");

	/* TODO: add _old_ state to message */
	CheckResultMessage params;
	params.SetService(GetName());
	params.SetCheckResult(cr);

	rm.SetParams(params);

	EndpointManager::GetInstance()->SendMulticastMessage(rm);

	if (call_eventhandler)
		ExecuteEventHandler();

	if (send_downtime_notification)
		RequestNotifications(in_downtime ? NotificationDowntimeStart : NotificationDowntimeEnd, cr);

	if (send_notification)
		RequestNotifications(recovery ? NotificationRecovery : NotificationProblem, cr);
}

ServiceState Service::StateFromString(const String& state)
{
	if (state == "OK")
		return StateOK;
	else if (state == "WARNING")
		return StateWarning;
	else if (state == "CRITICAL")
		return StateCritical;
	else if (state == "UNCHECKABLE")
		return StateUncheckable;
	else
		return StateUnknown;
}

String Service::StateToString(ServiceState state)
{
	switch (state) {
		case StateOK:
			return "OK";
		case StateWarning:
			return "WARNING";
		case StateCritical:
			return "CRITICAL";
		case StateUncheckable:
			return "UNCHECKABLE";
		case StateUnknown:
		default:
			return "UNKNOWN";
	}
}

StateType Service::StateTypeFromString(const String& type)
{
	if (type == "SOFT")
		return StateTypeSoft;
	else
		return StateTypeHard;
}

String Service::StateTypeToString(StateType type)
{
	if (type == StateTypeSoft)
		return "SOFT";
	else
		return "HARD";
}

bool Service::IsAllowedChecker(const String& checker) const
{
	Array::Ptr checkers = GetCheckers();

	if (!checkers)
		return true;

	ObjectLock olock(checkers);

	BOOST_FOREACH(const Value& pattern, checkers) {
		if (Utility::Match(pattern, checker))
			return true;
	}

	return false;
}

void Service::ExecuteCheck(void)
{
	ASSERT(!OwnsLock());

	bool reachable = IsReachable();

	{
		ObjectLock olock(this);

		/* don't run another check if there is one pending */
		if (m_CheckRunning)
			return;

		m_CheckRunning = true;

		SetLastState(GetState());
		SetLastStateType(GetLastStateType());
		SetLastReachable(reachable);
	}

	/* keep track of scheduling info in case the check type doesn't provide its own information */
	Dictionary::Ptr checkInfo = boost::make_shared<Dictionary>();
	checkInfo->Set("schedule_start", GetNextCheck());
	checkInfo->Set("execution_start", Utility::GetTime());

	Service::Ptr self = GetSelf();

	Dictionary::Ptr result;

	try {
		result = GetCheckCommand()->Execute(GetSelf());
	} catch (const std::exception& ex) {
		std::ostringstream msgbuf;
		msgbuf << "Exception occured during check for service '"
		       << GetName() << "': " << boost::diagnostic_information(ex);
		String message = msgbuf.str();

		Log(LogWarning, "icinga", message);

		result = boost::make_shared<Dictionary>();
		result->Set("state", StateUnknown);
		result->Set("output", message);
	}

	checkInfo->Set("execution_end", Utility::GetTime());
	checkInfo->Set("schedule_end", Utility::GetTime());
	checkInfo->Seal();

	if (result) {
		if (!result->Contains("schedule_start"))
			result->Set("schedule_start", checkInfo->Get("schedule_start"));

		if (!result->Contains("schedule_end"))
			result->Set("schedule_end", checkInfo->Get("schedule_end"));

		if (!result->Contains("execution_start"))
			result->Set("execution_start", checkInfo->Get("execution_start"));

		if (!result->Contains("execution_end"))
			result->Set("execution_end", checkInfo->Get("execution_end"));

		if (!result->Contains("macros"))
			result->Set("macros", checkInfo->Get("macros"));

		if (!result->Contains("active"))
			result->Set("active", 1);

		if (!result->Contains("current_checker"))
			result->Set("current_checker", EndpointManager::GetInstance()->GetIdentity());
	}

	if (result)
		ProcessCheckResult(result);

	/* figure out when the next check is for this service; the call to
	 * ProcessCheckResult() should've already done this but lets do it again
	 * just in case there was no check result. */
	UpdateNextCheck();

	{
		ObjectLock olock(this);
		m_CheckRunning = false;
	}
}

void Service::UpdateStatistics(const Dictionary::Ptr& cr)
{
	time_t ts;
	Value schedule_end = cr->Get("schedule_end");
	if (!schedule_end.IsEmpty())
		ts = static_cast<time_t>(schedule_end);
	else
		ts = static_cast<time_t>(Utility::GetTime());

	Value active = cr->Get("active");
	if (active.IsEmpty() || static_cast<long>(active))
		CIB::UpdateActiveChecksStatistics(ts, 1);
	else
		CIB::UpdatePassiveChecksStatistics(ts, 1);
}

double Service::CalculateExecutionTime(const Dictionary::Ptr& cr)
{
	double execution_start = 0, execution_end = 0;

	if (cr) {
		if (!cr->Contains("execution_start") || !cr->Contains("execution_end"))
			return 0;

		execution_start = cr->Get("execution_start");
		execution_end = cr->Get("execution_end");
	}

	return (execution_end - execution_start);
}

double Service::CalculateLatency(const Dictionary::Ptr& cr)
{
	double schedule_start = 0, schedule_end = 0;

	if (cr) {
		if (!cr->Contains("schedule_start") || !cr->Contains("schedule_end"))
			return 0;

		schedule_start = cr->Get("schedule_start");
		schedule_end = cr->Get("schedule_end");
	}

	return (schedule_end - schedule_start) - CalculateExecutionTime(cr);
}
