using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Runtime.Versioning;
using System.Threading;

namespace COM_Nostr.Internal;

[SupportedOSPlatform("windows")]
internal sealed class ComCallbackDispatcher : IDisposable
{
    private readonly SynchronizationContext? _context;
    private readonly object _gate = new();
    private readonly Queue<Action> _pending = new();
    private readonly BlockingCollection<Action>? _staQueue;
    private readonly Thread? _staThread;

    private bool _drainScheduled;
    private int _disposed;

    public ComCallbackDispatcher(SynchronizationContext? context)
    {
        _context = context;

        if (_context is null)
        {
            _staQueue = new BlockingCollection<Action>();
            _staThread = new Thread(StaPump)
            {
                IsBackground = true,
                Name = "ComCallbackDispatcherSTA"
            };
            _staThread.SetApartmentState(ApartmentState.STA);
            _staThread.Start();
        }
    }

    public void Post(Action action)
    {
        if (action is null)
        {
            throw new ArgumentNullException(nameof(action));
        }

        if (Volatile.Read(ref _disposed) == 1)
        {
            return;
        }

        if (_context is null)
        {
            try
            {
                _staQueue!.Add(action);
            }
            catch (InvalidOperationException)
            {
                // queue closed during disposal
            }
            return;
        }

        var shouldSchedule = false;
        lock (_gate)
        {
            if (_disposed == 1)
            {
                return;
            }

            _pending.Enqueue(action);
            if (!_drainScheduled)
            {
                _drainScheduled = true;
                shouldSchedule = true;
            }
        }

        if (shouldSchedule)
        {
            _context.Post(static state => ((ComCallbackDispatcher)state!).Drain(), this);
        }
    }

    public void Dispose()
    {
        if (Interlocked.Exchange(ref _disposed, 1) == 1)
        {
            return;
        }

        if (_context is null)
        {
            _staQueue!.CompleteAdding();

            if (Thread.CurrentThread != _staThread)
            {
                try
                {
                    _staThread!.Join();
                }
                catch (ThreadStateException)
                {
                    // thread not running
                }
            }

            _staQueue.Dispose();
        }
        else
        {
            lock (_gate)
            {
                _pending.Clear();
                _drainScheduled = false;
            }
        }
    }

    private void Drain()
    {
        while (true)
        {
            Action? work = null;

            lock (_gate)
            {
                if (_pending.Count == 0)
                {
                    _drainScheduled = false;
                    break;
                }

                work = _pending.Dequeue();
            }

            try
            {
                work();
            }
            catch
            {
                // swallow to keep dispatcher alive
            }
        }
    }

    private void StaPump()
    {
        try
        {
            foreach (var work in _staQueue!.GetConsumingEnumerable())
            {
                try
                {
                    work();
                }
                catch
                {
                    // swallow to keep dispatcher alive
                }
            }
        }
        catch
        {
            // ignored: queue disposed during shutdown
        }
    }
}
