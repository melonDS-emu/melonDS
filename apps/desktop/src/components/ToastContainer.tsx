/**
 * ToastContainer — renders all active toasts as an overlay in the
 * bottom-right corner of the screen.
 */

import { useToast } from '../context/ToastContext';

export function ToastContainer() {
  const { toasts, removeToast } = useToast();

  if (toasts.length === 0) return null;

  return (
    <div className="fixed bottom-6 right-6 z-50 flex flex-col gap-3 pointer-events-none">
      {toasts.map((toast) => (
        <div
          key={toast.id}
          className="pointer-events-auto flex items-start gap-3 rounded-xl bg-gray-800 border border-gray-700 shadow-2xl px-4 py-3 min-w-64 max-w-sm animate-fade-in"
        >
          {toast.icon && (
            <span className="text-2xl leading-none mt-0.5 shrink-0">{toast.icon}</span>
          )}
          <div className="flex-1 min-w-0">
            <p className="text-sm font-semibold text-white leading-snug">{toast.message}</p>
            {toast.detail && (
              <p className="text-xs text-gray-400 mt-0.5 leading-snug">{toast.detail}</p>
            )}
          </div>
          <button
            onClick={() => removeToast(toast.id)}
            className="shrink-0 text-gray-500 hover:text-gray-300 transition-colors text-lg leading-none mt-0.5"
            aria-label="Dismiss"
          >
            ×
          </button>
        </div>
      ))}
    </div>
  );
}
