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
public class BridgeResponse
{
    public bool ok;
    public string type;
    public string message;
    public bool running;
    public GameSnapshotData snapshot;
    public CommandPayload command;
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
