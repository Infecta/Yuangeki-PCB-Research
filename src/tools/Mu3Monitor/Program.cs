using Mu3IO;
using System.Diagnostics;
using System.Runtime.InteropServices;

byte ledLevel = 255;
var rightCOnly = false;
var leftWadOnly = false;
var rightWadOnly = false;
for (var i = 0; i < args.Length; ++i)
{
    if (string.Equals(args[i], "--led-stress", StringComparison.OrdinalIgnoreCase)) continue;
    if (string.Equals(args[i], "--right-c-only", StringComparison.OrdinalIgnoreCase))
    {
        rightCOnly = true;
        continue;
    }
    if (string.Equals(args[i], "--left-wad-only", StringComparison.OrdinalIgnoreCase))
    {
        leftWadOnly = true;
        continue;
    }
    if (string.Equals(args[i], "--right-wad-only", StringComparison.OrdinalIgnoreCase))
    {
        rightWadOnly = true;
        continue;
    }
    if (!string.Equals(args[i], "--led-level", StringComparison.OrdinalIgnoreCase))
    {
        Console.Error.WriteLine($"Unknown argument: {args[i]}. Use --led-level 128, --led-stress, or --right-c-only.");
        Environment.ExitCode = 2;
        return;
    }
    if (++i >= args.Length || !byte.TryParse(args[i], out ledLevel))
    {
        Console.Error.WriteLine("--led-level requires a brightness from 0 to 255.");
        Environment.ExitCode = 2;
        return;
    }
}

if ((rightCOnly ? 1 : 0) + (leftWadOnly ? 1 : 0) + (rightWadOnly ? 1 : 0) > 1)
{
    Console.Error.WriteLine("Choose only one of --right-c-only, --left-wad-only, or --right-wad-only.");
    Environment.ExitCode = 2;
    return;
}

var ledStress = args.Any(argument =>
    string.Equals(argument, "--led-stress", StringComparison.OrdinalIgnoreCase) ||
    string.Equals(argument, "--led-level", StringComparison.OrdinalIgnoreCase) ||
    string.Equals(argument, "--right-c-only", StringComparison.OrdinalIgnoreCase) ||
    string.Equals(argument, "--left-wad-only", StringComparison.OrdinalIgnoreCase) ||
    string.Equals(argument, "--right-wad-only", StringComparison.OrdinalIgnoreCase));

// Immutable complete MU3 report, sent through the existing WinUSB API.
// This test bypasses the mutable game color cache without modifying MU3IO.NET.
var oneLampOnly = rightCOnly || leftWadOnly || rightWadOnly;
var constantLedReport = oneLampOnly ? new byte[33] : Enumerable.Repeat(ledLevel, 33).ToArray();
constantLedReport[0] = 0x44;
constantLedReport[1] = 0x4C;
constantLedReport[2] = 1;
if (rightCOnly)
{
    // Sixth MU3 RGB triplet maps to the sixth main lamp: Right C.
    Array.Fill(constantLedReport, ledLevel, 3 + 5 * 3, 3);
}
else if (leftWadOnly)
{
    // First board-0 edge color is Ontroller packet triplet 6.
    Array.Fill(constantLedReport, ledLevel, 3 + 6 * 3, 3);
}
else if (rightWadOnly)
{
    // Last board-0 edge color is Ontroller packet triplet 9.
    Array.Fill(constantLedReport, ledLevel, 3 + 9 * 3, 3);
}

var lampDescription = rightCOnly ? "Right C only" :
                      leftWadOnly ? "Left WAD only" :
                      rightWadOnly ? "Right WAD only" : "all mapped lamps";

Console.WriteLine("Yuan'tGeki MU3IO.NET input monitor");
Console.WriteLine("Close the game, translator, and WebUI first; WinUSB access is exclusive.");
Console.WriteLine(ledStress
    ? $"LED stress ON: {lampDescription}, constant RGB {ledLevel}/{ledLevel}/{ledLevel}, refreshed every two input polls."
    : "LED stress OFF: the monitor sends no LED reports.");
Console.WriteLine("Every input transition is printed. Press Ctrl+C to stop.\n");

using var cancellation = new CancellationTokenSource();
Console.CancelKeyPress += (_, eventArgs) =>
{
    eventArgs.Cancel = true;
    cancellation.Cancel();
};

Snapshot? previous = null;
var reconnectMessageShown = false;
string? lastConnectionError = null;

while (!cancellation.IsCancellationRequested)
{
    Ontroller? controller = null;
    try
    {
        var device = DeviceManager.FindDevice(Ontroller.VendorId, Ontroller.ProductId);
        if (device is null)
        {
            if (!reconnectMessageShown)
            {
                Console.WriteLine($"{Timestamp()} Waiting for Ontroller VID 0E8F PID 1216...");
                reconnectMessageShown = true;
            }
            await Task.Delay(500, cancellation.Token);
            continue;
        }

        controller = new Ontroller(device);
        reconnectMessageShown = false;
        lastConnectionError = null;
        previous = null;
        Console.WriteLine($"{Timestamp()} Connected: {device.Name}");
        var timing = Stopwatch.StartNew();
        var lastWriteMs = 0L;
        var nextSummaryMs = 5000L;
        var maxWriteGapMs = 0L;
        var writes = 0L;
        var pollCount = 0;
        if (ledStress)
        {
            WriteConstantLeds(controller, constantLedReport);
            Console.WriteLine($"{Timestamp()} Initial LED write OK: {constantLedReport.Length} bytes");
            lastWriteMs = timing.ElapsedMilliseconds;
            ++writes;
        }

        while (!cancellation.IsCancellationRequested)
        {
            if (!controller.Poll(refreshLeds: false))
                throw new IOException("WinUSB input read failed");

            if (ledStress && ++pollCount == 2)
            {
                pollCount = 0;
                WriteConstantLeds(controller, constantLedReport);
                var now = timing.ElapsedMilliseconds;
                var gap = now - lastWriteMs;
                maxWriteGapMs = Math.Max(maxWriteGapMs, gap);
                if (gap >= 200)
                    Console.WriteLine($"{Timestamp()} LED WRITE GAP {gap} ms (STM32 host timeout: 250 ms)");
                lastWriteMs = now;
                ++writes;
                if (now >= nextSummaryMs)
                {
                    Console.WriteLine($"{Timestamp()} LED writes OK={writes}, largest completion gap={maxWriteGapMs} ms, constant level={ledLevel}");
                    nextSummaryMs = now + 5000;
                }
            }

            var current = Snapshot.From(controller);
            if (previous is null || current != previous)
            {
                PrintTransition(current, previous);
                previous = current;
            }
        }
    }
    catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
    {
        break;
    }
    catch (Exception exception)
    {
        if (exception.Message != lastConnectionError)
        {
            Console.WriteLine($"{Timestamp()} Disconnected/error: {exception.Message}");
            lastConnectionError = exception.Message;
        }
        reconnectMessageShown = true;
        try
        {
            await Task.Delay(500, cancellation.Token);
        }
        catch (OperationCanceledException)
        {
            break;
        }
    }
    finally
    {
        controller?.Dispose();
    }
}

static string Timestamp() => DateTime.Now.ToString("HH:mm:ss.fff");

static void WriteConstantLeds(Ontroller controller, byte[] report)
{
    var success = controller.WriteOutputData(report, out var transferred);
    var error = Marshal.GetLastWin32Error();
    if (!success || transferred != report.Length)
        throw new IOException($"LED USB write failed: success={success}, bytes={transferred}/{report.Length}, Win32={error}");
}

static void PrintTransition(Snapshot current, Snapshot? previous)
{
    var changed = previous is null
        ? "initial"
        : $"changed={(current.RawButtons0 ^ previous.RawButtons0):X2}/" +
          $"{(current.RawButtons1 ^ previous.RawButtons1):X2}";
    var warning = current.Service ? "  <<< SERVICE ASSERTED" :
                  current.Test ? "  <<< TEST ASSERTED" :
                  (current.RawSourceButtonsHigh & 0x0C) != 0
                      ? "  <<< RAW TEST/SERVICE GLITCH SUPPRESSED"
                      : string.Empty;

    Console.WriteLine(
        $"{Timestamp()} raw={current.RawButtons0:X2} {current.RawButtons1:X2} " +
        $"stmHi={current.RawSourceButtonsHigh:X2} " +
        $"lever={current.RawLever,3} scaled={current.ScaledLever,6} " +
        $"{changed}  {current.ActiveNames()}{warning}");
}

internal sealed record Snapshot(
    byte RawButtons0,
    byte RawButtons1,
    byte RawSourceButtonsHigh,
    ushort RawLever,
    short ScaledLever,
    byte Left,
    byte Right,
    byte Options)
{
    public bool Test => (Options & 0x01) != 0;
    public bool Service => (Options & 0x02) != 0;

    public static Snapshot From(Ontroller controller) => new(
        controller.RawButtons0,
        controller.RawButtons1,
        controller.RawSourceButtonsHigh,
        controller.RawLeverValue,
        controller.LeverPosition,
        controller.LeftGameButtonsFlag,
        controller.RightGameButtonsFlag,
        controller.OptionButtonsFlag);

    public string ActiveNames()
    {
        var names = new List<string>();
        Add(Left, 0x01, "L1");
        Add(Left, 0x02, "L2");
        Add(Left, 0x04, "L3");
        Add(Left, 0x08, "LSide");
        Add(Left, 0x10, "LMenu");
        Add(Right, 0x01, "R1");
        Add(Right, 0x02, "R2");
        Add(Right, 0x04, "R3");
        Add(Right, 0x08, "RSide");
        Add(Right, 0x10, "RMenu");
        Add(Options, 0x01, "Test");
        Add(Options, 0x02, "Service");
        return names.Count == 0 ? "none" : string.Join(',', names);

        void Add(byte value, byte mask, string name)
        {
            if ((value & mask) != 0) names.Add(name);
        }
    }
}
