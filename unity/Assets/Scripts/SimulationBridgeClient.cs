using System;
using System.Diagnostics;
using System.IO;
using UnityEngine;

[Serializable]
public class DecisionPointData
{
    public bool awaiting_command;
    public int offense_team_index;
    public int defense_team_index;
}

[Serializable]
public class GameSnapshotData
{
    public string game_id;
    public int quarter;
    public float game_clock;
    public float shot_clock;
    public int home_score;
    public int away_score;
    public int home_fouls;
    public int away_fouls;
    public int home_timeouts;
    public int away_timeouts;
    public string possession;
    public string ball_handler;
    public bool game_over;
    public string[] latest_events;
    public DecisionPointData decision_point;
}

[Serializable]
public class CommandPayload
{
    public bool accepted;
    public string reason;
}

[Serializable]
public class ScheduledGameData
{
    public string game_id;
    public string home_team_id;
    public string away_team_id;
    public int day;
    public bool simulated;
    public int home_score;
    public int away_score;
}

[Serializable]
public class TeamStandingData
{
    public string team_id;
    public int wins;
    public int losses;
    public int points_for;
    public int points_against;
}

[Serializable]
public class RookieProspectData
{
    public string prospect_id;
    public string name;
    public int age;
    public int position;
    public float rating;
    public float potential;
}

[Serializable]
public class TradeResultData
{
    public bool accepted;
    public string reason;
    public float value_sent;
    public float value_received;
}

[Serializable]
public class BridgeResponse
{
    public bool ok;
    public string type;
    public string message;
    public bool running;
    public GameSnapshotData snapshot;
    public CommandPayload command;

    // Franchise fields
    public int current_day;
    public string[] logs;
    public TeamStandingData[] standings;
    public ScheduledGameData[] schedule;
    public TradeResultData result;
    public bool success;
    public RookieProspectData[] prospects;
    public string[] order;
    public string drafted_name;
}

public class SimulationBridgeClient : MonoBehaviour
{
    [Header("Path to bridge executable")]
    public string bridgeExecutablePath = "./bazel-bin/fastbreak_bridge";

    private Process _process;

    private void OnDestroy()
    {
        Shutdown();
    }

    public bool StartBridge()
    {
        if (_process != null && !_process.HasExited)
        {
            return true;
        }

        if (!File.Exists(bridgeExecutablePath))
        {
            UnityEngine.Debug.LogError($"Bridge executable not found: {bridgeExecutablePath}");
            return false;
        }

        var startInfo = new ProcessStartInfo
        {
            FileName = bridgeExecutablePath,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        _process = new Process { StartInfo = startInfo };
        _process.Start();
        return true;
    }

    public GameSnapshotData InitGame()
    {
        BridgeResponse response = Send("init");
        return response.snapshot;
    }

    public GameSnapshotData GetSnapshot()
    {
        BridgeResponse response = Send("snapshot");
        return response.snapshot;
    }

    public BridgeResponse Step()
    {
        return Send("step");
    }

    public CommandPayload SendManagerCommand(string commandName)
    {
        BridgeResponse response = Send($"command {commandName}");
        return response.command;
    }

    // --- FRANCHISE MODE APIS ---
    
    public int InitFranchise()
    {
        BridgeResponse response = Send("franchise_init");
        return response.current_day;
    }

    public BridgeResponse SimulateFranchiseDay(string playerTeamId)
    {
        return Send($"franchise_simulate_day {playerTeamId}");
    }

    public TeamStandingData[] GetFranchiseStandings()
    {
        BridgeResponse response = Send("franchise_standings");
        return response.standings;
    }

    public ScheduledGameData[] GetFranchiseSchedule()
    {
        BridgeResponse response = Send("franchise_schedule");
        return response.schedule;
    }

    public TradeResultData EvaluateFranchiseTrade(string teamA, string teamB, string[] sentIds, string[] recvIds)
    {
        string sent = string.Join(",", sentIds);
        string recv = string.Join(",", recvIds);
        BridgeResponse response = Send($"franchise_trade_evaluate {teamA} {teamB} {sent} {recv}");
        return response.result;
    }

    public bool ExecuteFranchiseTrade(string teamA, string teamB, string[] sentIds, string[] recvIds)
    {
        string sent = string.Join(",", sentIds);
        string recv = string.Join(",", recvIds);
        BridgeResponse response = Send($"franchise_trade_execute {teamA} {teamB} {sent} {recv}");
        return response.success;
    }

    public RookieProspectData[] GetDraftClass()
    {
        BridgeResponse response = Send("franchise_draft_class");
        return response.prospects;
    }

    public string[] GetDraftOrder()
    {
        BridgeResponse response = Send("franchise_draft_order");
        return response.order;
    }

    public BridgeResponse ExecuteDraftPick(string teamId, string prospectId, string type)
    {
        return Send($"franchise_draft_pick {teamId} {prospectId} {type}");
    }

    public void Shutdown()
    {
        if (_process == null)
        {
            return;
        }

        try
        {
            if (!_process.HasExited)
            {
                _process.StandardInput.WriteLine("quit");
                _process.StandardInput.Flush();
                _process.Kill();
            }
        }
        catch (Exception ex)
        {
            UnityEngine.Debug.LogWarning($"Bridge shutdown issue: {ex.Message}");
        }

        _process.Dispose();
        _process = null;
    }

    private BridgeResponse Send(string command)
    {
        if (_process == null || _process.HasExited)
        {
            throw new Exception("Bridge process is not running.");
        }

        _process.StandardInput.WriteLine(command);
        _process.StandardInput.Flush();

        string line = _process.StandardOutput.ReadLine();
        if (string.IsNullOrEmpty(line))
        {
            string err = _process.StandardError.ReadToEnd();
            throw new Exception($"No response from bridge. stderr: {err}");
        }

        BridgeResponse response = JsonUtility.FromJson<BridgeResponse>(line);
        if (!response.ok)
        {
            throw new Exception($"Bridge error: {response.message}");
        }

        return response;
    }
}
