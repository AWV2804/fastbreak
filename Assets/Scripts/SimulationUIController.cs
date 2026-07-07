using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using TMPro;

public class SimulationUIController : MonoBehaviour
{
    [Header("Bridge Connection")]
    public SimulationBridgeClient bridge;

    [Header("Scoreboard Text Elements")]
    public TMP_Text gameClockText;
    public TMP_Text shotClockText;
    public TMP_Text quarterText;
    public TMP_Text homeScoreText;
    public TMP_Text awayScoreText;
    public TMP_Text homeFoulsText;
    public TMP_Text awayFoulsText;
    public TMP_Text homeTimeoutsText;
    public TMP_Text awayTimeoutsText;
    public TMP_Text possessionText;
    public TMP_Text statusText;

    [Header("Tactics Interactive Elements")]
    public Button timeoutButton;
    public Slider paceSlider; // 0 = slow, 1 = neutral, 2 = fast
    public Dropdown offenseDropdown; // 0 = balanced, 1 = inside, 2 = perimeter
    public Dropdown defenseDropdown; // 0 = man-to-man, 1 = zone, 2 = press

    [Header("Play-by-Play Event Log")]
    public ScrollRect eventLogScrollRect;
    public TMP_Text eventLogPrefabText;
    public RectTransform eventLogContainer;

    [Header("Box Score Panels")]
    public RectTransform homeRosterContainer;
    public RectTransform awayRosterContainer;
    public GameObject boxScoreRowPrefab; // Prefab with labels for Name, Min, PTS, REB, AST, etc.

    private GameSnapshotData _lastSnapshot;
    private List<GameObject> _spawnedBoxScoreRows = new List<GameObject>();

    private void Start()
    {
        if (bridge == null)
        {
            bridge = GetComponent<SimulationBridgeClient>();
        }

        // Setup button listeners
        if (timeoutButton != null)
        {
            timeoutButton.onClick.AddListener(CallTimeout);
        }

        if (paceSlider != null)
        {
            paceSlider.onValueChanged.AddListener(OnPaceChanged);
        }

        if (offenseDropdown != null)
        {
            offenseDropdown.onValueChanged.AddListener(OnOffenseChanged);
        }

        if (defenseDropdown != null)
        {
            defenseDropdown.onValueChanged.AddListener(OnDefenseChanged);
        }

        // Initialize simulation
        if (bridge != null && bridge.StartBridge())
        {
            _lastSnapshot = bridge.InitGame();
            UpdateUI();
        }
    }

    public void StepSimulation()
    {
        if (bridge == null || (_lastSnapshot != null && _lastSnapshot.game_over))
        {
            return;
        }

        try
        {
            var response = bridge.Step();
            _lastSnapshot = response.snapshot;
            
            if (statusText != null)
            {
                statusText.text = response.running ? "Active" : "Game Finished";
            }

            UpdateUI();
        }
        catch (Exception ex)
        {
            Debug.LogError("Error stepping simulation: " + ex.Message);
        }
    }

    public void CallTimeout()
    {
        if (bridge == null) return;
        var result = bridge.SendManagerCommand("timeout");
        if (statusText != null)
        {
            statusText.text = result.accepted ? "Timeout called!" : $"Timeout rejected: {result.reason}";
        }
        RefreshGameSnapshot();
    }

    private void OnPaceChanged(float val)
    {
        if (bridge == null) return;
        string pace = "neutral";
        if (val == 0) pace = "slow";
        else if (val == 2) pace = "fast";

        bridge.SendManagerCommand($"pace {pace}");
    }

    private void OnOffenseChanged(int index)
    {
        if (bridge == null) return;
        string focus = "balanced";
        if (index == 1) focus = "inside";
        else if (index == 2) focus = "perimeter";

        bridge.SendManagerCommand($"focus {focus}");
    }

    private void OnDefenseChanged(int index)
    {
        if (bridge == null) return;
        string scheme = "man";
        if (index == 1) scheme = "zone";
        else if (index == 2) scheme = "press";

        bridge.SendManagerCommand($"defense {scheme}");
    }

    private void RefreshGameSnapshot()
    {
        try
        {
            _lastSnapshot = bridge.GetSnapshot();
            UpdateUI();
        }
        catch (Exception ex)
        {
            Debug.LogError("Error updating snapshot: " + ex.Message);
        }
    }

    private void UpdateUI()
    {
        if (_lastSnapshot == null) return;

        // 1. Update Scoreboard Clocks
        int minutes = Mathf.FloorToInt(_lastSnapshot.game_clock / 60.0f);
        int seconds = Mathf.FloorToInt(_lastSnapshot.game_clock % 60.0f);
        
        if (gameClockText != null) gameClockText.text = $"{minutes:00}:{seconds:00}";
        if (shotClockText != null) shotClockText.text = $"{_lastSnapshot.shot_clock:0.0}s";
        if (quarterText != null) quarterText.text = $"QTR {_lastSnapshot.quarter}";
        
        // Scores
        if (homeScoreText != null) homeScoreText.text = _lastSnapshot.home_score.ToString();
        if (awayScoreText != null) awayScoreText.text = _lastSnapshot.away_score.ToString();

        // Fouls & Timeouts
        if (homeFoulsText != null) homeFoulsText.text = $"Fouls: {_lastSnapshot.home_fouls}";
        if (awayFoulsText != null) awayFoulsText.text = $"Fouls: {_lastSnapshot.away_fouls}";
        if (homeTimeoutsText != null) homeTimeoutsText.text = $"Timeouts: {_lastSnapshot.home_timeouts}";
        if (awayTimeoutsText != null) awayTimeoutsText.text = $"Timeouts: {_lastSnapshot.away_timeouts}";

        // Possession & Handler
        if (possessionText != null)
        {
            possessionText.text = $"Ball: {_lastSnapshot.possession} (Handler: {_lastSnapshot.ball_handler})";
        }

        // 2. Update Play-by-Play event list
        UpdateEventLog();

        // 3. Update Box Score grids (Mocking row additions, in real game you can query statistics or pass list)
        UpdateRosterBoxScore();
    }

    private void UpdateEventLog()
    {
        if (eventLogContainer == null || eventLogPrefabText == null || _lastSnapshot.latest_events == null)
        {
            return;
        }

        // Clear existing log items
        foreach (Transform child in eventLogContainer)
        {
            Destroy(child.gameObject);
        }

        // Spawn new logs
        foreach (var evt in _lastSnapshot.latest_events)
        {
            TMP_Text newItem = Instantiate(eventLogPrefabText, eventLogContainer);
            newItem.text = evt;
            newItem.gameObject.SetActive(true);
        }

        // Auto Scroll to bottom
        Canvas.ForceUpdateCanvases();
        if (eventLogScrollRect != null)
        {
            eventLogScrollRect.verticalNormalizedPosition = 0f;
        }
    }

    private void UpdateRosterBoxScore()
    {
        // Simple cleanup of spawned rows
        foreach (var row in _spawnedBoxScoreRows)
        {
            Destroy(row);
        }
        _spawnedBoxScoreRows.Clear();

        if (boxScoreRowPrefab == null) return;

        // In a detailed UI implementation, you would bind data matching the player game statistics 
        // that can be obtained by adding a "roster_stats" array in SimulationEngine/JSON snapshots.
    }
}
