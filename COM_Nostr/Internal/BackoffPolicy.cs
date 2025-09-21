using System;
using System.Threading;

namespace COM_Nostr.Internal;

internal sealed class BackoffPolicy
{
    private readonly TimeSpan _initialDelay;
    private readonly TimeSpan _maximumDelay;
    private readonly double _multiplier;
    private int _attempt;

    public BackoffPolicy(TimeSpan initialDelay, TimeSpan maximumDelay, double multiplier = 2.0)
    {
        if (initialDelay < TimeSpan.Zero)
        {
            throw new ArgumentOutOfRangeException(nameof(initialDelay), "Initial delay must be non-negative.");
        }

        if (maximumDelay < initialDelay)
        {
            throw new ArgumentOutOfRangeException(nameof(maximumDelay), "Maximum delay must be greater than or equal to the initial delay.");
        }

        if (multiplier < 1.0)
        {
            throw new ArgumentOutOfRangeException(nameof(multiplier), "Multiplier must be at least 1.0.");
        }

        _initialDelay = initialDelay;
        _maximumDelay = maximumDelay;
        _multiplier = multiplier;
    }

    public TimeSpan GetNextDelay()
    {
        var attempt = Interlocked.Increment(ref _attempt);
        var factor = Math.Pow(_multiplier, Math.Max(0, attempt - 1));
        var milliseconds = _initialDelay.TotalMilliseconds * factor;
        var clamped = Math.Min(milliseconds, _maximumDelay.TotalMilliseconds);
        return TimeSpan.FromMilliseconds(clamped);
    }

    public void Reset()
    {
        Interlocked.Exchange(ref _attempt, 0);
    }
}
