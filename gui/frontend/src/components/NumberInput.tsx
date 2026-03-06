import { useState, useEffect } from 'react'

export default function NumberInput({
  value,
  onChange,
  ...props
}: {
  value: number
  onChange: (v: number) => void
} & Omit<React.InputHTMLAttributes<HTMLInputElement>, 'value' | 'onChange' | 'type'>) {
  const [text, setText] = useState(String(value))
  const [focused, setFocused] = useState(false)

  useEffect(() => {
    if (!focused) setText(String(value))
  }, [value, focused])

  const commit = () => {
    const parsed = parseFloat(text)
    if (!isNaN(parsed)) onChange(parsed)
    else setText(String(value))
  }

  return (
    <input
      {...props}
      type="text"
      inputMode="decimal"
      value={focused ? text : value}
      onChange={(e) => setText(e.target.value)}
      onFocus={() => setFocused(true)}
      onBlur={() => { setFocused(false); commit() }}
      onKeyDown={(e) => { if (e.key === 'Enter') commit() }}
    />
  )
}
