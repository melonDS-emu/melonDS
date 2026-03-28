/**
 * ToastContainer — Nintendo-style notification toasts, fixed bottom-right.
 */

import { useToast } from '../context/ToastContext';

export function ToastContainer() {
  const { toasts, removeToast } = useToast();

  if (toasts.length === 0) return null;

  return (
    <div className="fixed bottom-6 right-6 z-50 flex flex-col gap-2.5 pointer-events-none">
      {toasts.map((toast) => (
        <div
          key={toast.id}
          className="pointer-events-auto flex items-start gap-3 rounded-2xl shadow-2xl px-4 py-3 min-w-64 max-w-sm"
          style={{
            backgroundColor: 'var(--color-oasis-card)',
            border: '1px solid var(--n-border)',
            boxShadow: '0 8px 32px rgba(0,0,0,0.5)',
          }}
        >
          {toast.icon && (
            <span className="text-2xl leading-none mt-0.5 shrink-0">{toast.icon}</span>
          )}
          <div className="flex-1 min-w-0">
            <p className="text-sm font-black text-white leading-snug">{toast.message}</p>
            {toast.detail && (
              <p className="text-xs mt-0.5 leading-snug" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {toast.detail}
              </p>
            )}
          </div>
          <button
            onClick={() => removeToast(toast.id)}
            className="shrink-0 transition-colors text-lg leading-none mt-0.5 font-bold"
            style={{ color: 'var(--color-oasis-text-muted)' }}
            aria-label="Dismiss"
          >
            ×
          </button>
        </div>
      ))}
    </div>
  );
}
