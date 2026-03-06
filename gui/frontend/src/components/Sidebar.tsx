import { NavLink } from 'react-router-dom'

interface NavItem {
  path: string
  label: string
  icon: JSX.Element
}

const primaryNavItems: NavItem[] = [
  {
    path: '/',
    label: 'Pipeline',
    icon: (
      <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M5.25 5.653c0-.856.917-1.398 1.667-.986l11.54 6.348a1.125 1.125 0 010 1.971l-11.54 6.347a1.125 1.125 0 01-1.667-.985V5.653z" />
      </svg>
    ),
  },
  {
    path: '/batch',
    label: 'Batch',
    icon: (
      <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M2.25 12.75V12A2.25 2.25 0 014.5 9.75h15A2.25 2.25 0 0121.75 12v.75m-8.69-6.44l-2.12-2.12a1.5 1.5 0 00-1.061-.44H4.5A2.25 2.25 0 002.25 6v12a2.25 2.25 0 002.25 2.25h15A2.25 2.25 0 0021.75 18V9a2.25 2.25 0 00-2.25-2.25h-5.379a1.5 1.5 0 01-1.06-.44z" />
      </svg>
    ),
  },
  {
    path: '/history',
    label: 'History',
    icon: (
      <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M12 6v6h4.5m4.5 0a9 9 0 11-18 0 9 9 0 0118 0z" />
      </svg>
    ),
  },
]


function NavItemLink({ item }: { item: NavItem }) {
  return (
    <NavLink
      to={item.path}
      end={item.path === '/'}
      className={({ isActive }) =>
        `flex items-center gap-3 rounded-lg px-3 py-2.5 text-sm font-medium transition-colors duration-150 ${
          isActive
            ? 'bg-primary-600/20 text-primary-400 shadow-sm'
            : 'text-gray-400 hover:bg-gray-800 hover:text-gray-200'
        }`
      }
    >
      {item.icon}
      {item.label}
    </NavLink>
  )
}

export default function Sidebar() {
  return (
    <aside className="fixed inset-y-0 left-0 z-30 flex w-60 flex-col bg-gray-900 border-r border-gray-800">
      {/* Logo / Brand */}
      <div className="flex h-16 items-center gap-3 px-5 border-b border-gray-800">
        <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-primary-600">
          <svg className="w-5 h-5 text-white" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M21 7.5l-9-5.25L3 7.5m18 0l-9 5.25m9-5.25v9l-9 5.25M3 7.5l9 5.25M3 7.5v9l9 5.25m0-9v9" />
          </svg>
        </div>
        <div>
          <h1 className="text-base font-bold text-white tracking-tight">FiberFoam</h1>
          <p className="text-[10px] text-gray-500 uppercase tracking-widest">Simulation Suite</p>
        </div>
      </div>

      {/* Navigation */}
      <nav className="flex-1 overflow-y-auto px-3 py-4">
        {/* Primary nav */}
        <div className="space-y-1">
          {primaryNavItems.map((item) => (
            <NavItemLink key={item.path} item={item} />
          ))}
        </div>

      </nav>

      {/* Footer */}
      <div className="border-t border-gray-800 px-5 py-3">
        <p className="text-xs text-gray-600">v0.1.0</p>
      </div>
    </aside>
  )
}
