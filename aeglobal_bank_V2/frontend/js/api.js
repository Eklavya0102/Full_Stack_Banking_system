// api.js — Shared fetch wrapper for AE Global Bank
// Auto-detects backend port by trying 8080, 8081, 8082, 3000

const PORTS = [8080, 8081, 8082, 3000];
let API_BASE = 'http://localhost:8080/api'; // default, overridden on init

// Detect which port the backend is running on
async function detectPort() {
    // Try to read port.txt written by the server on startup
    try {
        const res = await fetch('../port.txt', { cache: 'no-store' });
        if (res.ok) {
            const port = parseInt((await res.text()).trim());
            if (port > 0) {
                API_BASE = `http://localhost:${port}/api`;
                console.log(`[API] Using port from port.txt: ${port}`);
                return;
            }
        }
    } catch(_) {}

    // Fallback: probe each port with a quick /api/stats call
    for (const port of PORTS) {
        try {
            const r = await fetch(`http://localhost:${port}/api/stats`,
                { signal: AbortSignal.timeout(800) });
            if (r.ok) {
                API_BASE = `http://localhost:${port}/api`;
                console.log(`[API] Auto-detected port: ${port}`);
                return;
            }
        } catch(_) {}
    }
    console.warn('[API] Could not detect port — defaulting to 8080');
}

// Run detection immediately; all API calls await this promise
const portReady = detectPort();

function getSession() {
    const s = localStorage.getItem('ae_session');
    return s ? JSON.parse(s) : null;
}
function setSession(data) { localStorage.setItem('ae_session', JSON.stringify(data)); }
function clearSession()    { localStorage.removeItem('ae_session'); }

function authHeaders() {
    const session = getSession();
    if (!session) return { 'Content-Type': 'application/json' };
    return {
        'Content-Type':   'application/json',
        'Authorization':  `Bearer ${session.token}`,
        'X-User-Email':   session.user.email,
        'X-User-Role':    session.user.role
    };
}

async function apiCall(method, endpoint, body = null) {
    await portReady;   // wait for port detection before any request
    const opts = { method, headers: authHeaders() };
    if (body) opts.body = JSON.stringify(body);
    try {
        const res  = await fetch(`${API_BASE}${endpoint}`, opts);
        const data = await res.json();
        return { ok: res.ok, status: res.status, data };
    } catch (e) {
        return {
            ok: false, status: 0,
            data: { error: 'Cannot connect to server. Is banking_server.exe running?' }
        };
    }
}

const API = {
    login:  (email, password, role) =>
        apiCall('POST', '/login', { email, password, role }),

    // Admin
    getStaff:       ()           => apiCall('GET',  '/admin/staff'),
    createStaff:    (data)       => apiCall('POST', '/admin/staff', data),
    deleteStaff:    (id)         => apiCall('DELETE', `/admin/staff/${id}`),
    getRequests:    (filter='pending') =>
        apiCall('GET', `/admin/requests?filter=${filter}`),
    approveRequest: (id)         => apiCall('POST', `/admin/approve/${id}`),
    rejectRequest:  (id, reason) =>
        apiCall('POST', `/admin/reject/${id}`, { reason }),
    getStats:       ()           => apiCall('GET',  '/stats'),

    // Account
    openAccountRequest: (data)       => apiCall('POST', '/account/request', data),
    searchAccounts:     (q)          =>
        apiCall('GET', `/account/search?q=${encodeURIComponent(q)}`),
    getBalance:         (num, byCard = false) =>
        apiCall('GET', byCard
            ? `/account/balance?card_number=${encodeURIComponent(num)}`
            : `/account/balance?account_number=${encodeURIComponent(num)}`),
    closeAccount:       (num)        =>
        apiCall('POST', '/account/close', { account_number: num }),
    getCard:            (num)        =>
        apiCall('GET', `/account/card?account_number=${num}`),

    // Transactions
    deposit:         (num, amt) =>
        apiCall('POST', '/transaction/deposit',  { account_number: num, amount: amt }),
    withdraw:        (num, amt) =>
        apiCall('POST', '/transaction/withdraw', { account_number: num, amount: amt }),
    getTransactions: (num = '') =>
        apiCall('GET', `/transactions${num ? '?account_number=' + num : ''}`),
};

// ── UI helpers ────────────────────────────────────────────────────────────────

function showToast(msg, type = 'success') {
    let container = document.getElementById('toast-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        container.style.cssText =
            'position:fixed;top:80px;right:20px;z-index:9999;' +
            'display:flex;flex-direction:column;gap:10px;';
        document.body.appendChild(container);
    }
    const colors = { success:'#22c55e', error:'#ef4444', info:'#3b82f6', warn:'#f59e0b' };
    const icons  = { success:'check_circle', error:'error', info:'info', warn:'warning' };
    const toast  = document.createElement('div');
    toast.style.cssText =
        `background:white;border-left:4px solid ${colors[type]||colors.info};` +
        `padding:14px 18px;border-radius:8px;box-shadow:0 4px 20px rgba(0,0,0,0.12);` +
        `font-family:'Public Sans',sans-serif;font-size:14px;max-width:340px;` +
        `animation:slideIn 0.3s ease;display:flex;align-items:center;gap:10px;`;
    toast.innerHTML =
        `<span class="material-symbols-outlined" style="color:${colors[type]};font-size:20px">` +
        `${icons[type]||'info'}</span>` +
        `<span style="color:#1a1c1e">${msg}</span>`;
    container.appendChild(toast);
    setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transition = 'opacity 0.3s';
        setTimeout(() => toast.remove(), 300);
    }, 3500);
}

function formatCurrency(amount) {
    // Indian Rupee format: ₹1,00,000  ₹10,50,500  ₹1,25,00,000
    if (amount === null || amount === undefined || isNaN(amount)) return '₹0.00';
    return new Intl.NumberFormat('en-IN', {
        style:                 'currency',
        currency:              'INR',
        minimumFractionDigits: 2,
        maximumFractionDigits: 2
    }).format(amount);
}

function formatDate(dateStr) {
    if (!dateStr) return '—';
    return new Date(dateStr).toLocaleDateString('en-US',
        { year:'numeric', month:'short', day:'numeric',
          hour:'2-digit', minute:'2-digit' });
}

function statusBadge(status) {
    const map = {
        active:    'bg-green-100 text-green-700 border-green-200',
        closed:    'bg-gray-100 text-gray-600 border-gray-200',
        suspended: 'bg-yellow-100 text-yellow-700 border-yellow-200',
        pending:   'bg-amber-100 text-amber-700 border-amber-200',
        approved:  'bg-green-100 text-green-700 border-green-200',
        rejected:  'bg-red-100 text-red-700 border-red-200',
        blocked:   'bg-red-100 text-red-700 border-red-200',
        expired:   'bg-gray-100 text-gray-500 border-gray-200',
    };
    const cls = map[status] || 'bg-gray-100 text-gray-600 border-gray-200';
    return `<span class="px-2 py-0.5 rounded-full text-[11px] font-bold uppercase border ${cls}">${status}</span>`;
}

// Toast animation CSS
const _s = document.createElement('style');
_s.textContent = `
@keyframes slideIn {
    from { transform:translateX(20px); opacity:0; }
    to   { transform:translateX(0);    opacity:1; }
}
.panel { display: none !important; }
.panel.active { display: block !important; }
.nav-item.active {
    background: #f1f5f9; color: #00234b !important;
    font-weight: 700; border-left: 4px solid #00234b;
}
`;
document.head.appendChild(_s);
