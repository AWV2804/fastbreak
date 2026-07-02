using UnityEngine;

public class SimulationDebugController : MonoBehaviour
{
    public SimulationBridgeClient bridge;

    private GameSnapshotData _snapshot;
    private string _lastCommandResult = "";
    private Vector2 _scrollPosition = Vector2.zero;

    private void Start()
    {
        if (bridge == null)
        {
            bridge = GetComponent<SimulationBridgeClient>();
        }

        if (bridge != null && bridge.StartBridge())
        {
            _snapshot = bridge.InitGame();
        }
    }

    private void Update()
    {
        if (bridge == null)
        {
            return;
        }

        if (Input.GetKeyDown(KeyCode.Space))
        {
            var response = bridge.Step();
            _snapshot = response.snapshot;
            _lastCommandResult = response.running ? "step ok" : "game complete";
        }

        if (Input.GetKeyDown(KeyCode.T))
        {
            var result = bridge.SendManagerCommand("timeout");
            _lastCommandResult = result.accepted ? "timeout called" : result.reason;
            RefreshSnapshot();
        }

        if (Input.GetKeyDown(KeyCode.P))
        {
            var result = bridge.SendManagerCommand("pace fast");
            _lastCommandResult = result.accepted ? "strategy: fast pace" : result.reason;
        }

        if (Input.GetKeyDown(KeyCode.O))
        {
            var result = bridge.SendManagerCommand("focus perimeter");
            _lastCommandResult = result.accepted ? "strategy: perimeter focus" : result.reason;
        }

        if (Input.GetKeyDown(KeyCode.D))
        {
            var result = bridge.SendManagerCommand("defense press");
            _lastCommandResult = result.accepted ? "strategy: press defense" : result.reason;
        }
    }

    private void RefreshSnapshot()
    {
        try
        {
            _snapshot = bridge.GetSnapshot();
        }
        catch (System.Exception ex)
        {
            UnityEngine.Debug.LogError("Error getting snapshot: " + ex.Message);
        }
    }

    private void OnGUI()
    {
        // Left Panel: Game State info
        GUILayout.BeginArea(new Rect(20, 20, 320, 480), GUI.skin.box);
        GUILayout.Label("FastBreak Basketball Unity Debug Panel", GUI.skin.horizontalSlider);

        if (_snapshot != null)
        {
            string min = Mathf.FloorToInt(_snapshot.game_clock / 60.0f).ToString("00");
            string sec = Mathf.FloorToInt(_snapshot.game_clock % 60.0f).ToString("00");

            GUILayout.Label($"Game ID: {_snapshot.game_id}");
            GUILayout.Label($"Quarter: {_snapshot.quarter}");
            GUILayout.Label($"Game Clock: {min}:{sec}");
            GUILayout.Label($"Shot Clock: {_snapshot.shot_clock:0.0}s");
            GUILayout.Label($"Score: Home {_snapshot.home_score} - {_snapshot.away_score} Away");
            GUILayout.Label($"Fouls: Home {_snapshot.home_fouls} - {_snapshot.away_fouls} Away");
            GUILayout.Label($"Timeouts: Home {_snapshot.home_timeouts} - {_snapshot.away_timeouts} Away");
            GUILayout.Label($"Possession: {_snapshot.possession}");
            GUILayout.Label($"Ball Handler: {_snapshot.ball_handler}");
            GUILayout.Label($"Game Over: {_snapshot.game_over}");
        }
        else
        {
            GUILayout.Label("No snapshot loaded. Ensure the bridge path is correct and game is initialized.");
        }

        GUILayout.Space(10);
        GUILayout.Label("Controls:\n- Space: Step Action\n- T: Timeout\n- P: Set Fast Pace\n- O: Set Perimeter Offense\n- D: Set Press Defense", GUI.skin.box);
        GUILayout.Label("Last Action Status: " + _lastCommandResult);
        GUILayout.EndArea();

        // Right Panel: Play-by-Play Event Log
        GUILayout.BeginArea(new Rect(360, 20, 420, 480), GUI.skin.box);
        GUILayout.Label("Live Play-by-Play Log", GUI.skin.horizontalSlider);
        
        _scrollPosition = GUILayout.BeginScrollView(_scrollPosition);
        if (_snapshot != null && _snapshot.latest_events != null)
        {
            foreach (var evt in _snapshot.latest_events)
            {
                GUILayout.Label(evt, GUI.skin.textArea);
            }
        }
        GUILayout.EndScrollView();
        GUILayout.EndArea();
    }
}
