-- Feedback only. Native CC Rel1 turns and default pushes remain in firmware.
-- No sound values, parameter tables, or encoder callbacks are stored here.
local seq=0
local function request()
  seq=seq%127+1
  midi.sendCC(3,13,125,seq)
  midi.sendCC(3,13,126,controller.getPage())
end
local function clear()
  for i=1,16 do leds.updateByIndex(i,0,0) end
end
function page.onInit()
  clear()
  system.setUpdateRate(1000)
  request()
end
function page.onPageChange(prev,curr)
  clear()
  request()
end
function system.update()
  -- Refresh also discovers Thru when it is opened after the scene.
  request()
end
function controller.onSysex(b)
  if #b<8 or (#b-8)%4~=0 or b[1]~=240 or b[#b]~=247
      or b[2]~=125 or b[3]~=83 or b[4]~=84 or b[5]~=1
      or b[6]~=controller.getPage()-1 or b[7]~=seq then return end
  for i=2,#b-1 do
    if type(b[i])~="number" or b[i]<0 or b[i]>127 then return end
  end
  for i=8,#b-1,4 do
    local index=b[i]+1
    if index>=1 and index<=16 and b[i+3]<=100 then
      leds.updateByIndex(index,b[i+1]*128+b[i+2],b[i+3])
    end
  end
end
