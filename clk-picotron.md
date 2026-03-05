

Analyser::Static::GetTargets() called from 

Narrows down the options for which emulator needs launching based on the input file.

CSMachine is the single machine/computer type instance. Created via `initWithAnalyser:`. Finally creates a machine in 

`std::unique_ptr<Machine::DynamicMachine> Machine::MachineForTarget()`

Finally resolves to 

`Electron::ConcreteMachine <hasScsi>`

`Electron::ConcreteMachine::run_for()` is primary emulation loop. The full emualation time step from real time to emulation time is in `MachineUpdater::perform()`. `set_scan_target()` appears to be where the machine is told where to output display data. `CSMachine setView: aspectRatio:` links the OS-level view with the machine's output. `VideoOutput video_` is the member of ConcreteMachine that handles the frame buffer.

`- (void)scanTargetViewDisplayLinkDidFire:(CSScanTargetView *)view` seems to be where the view is actually controlled, connecting a BufferingScanTarget with the actual Metal view.



Can I run my own specific machine instance with a constant clock rate and link it with _some_ kind of output? Just ticking along would be okay.