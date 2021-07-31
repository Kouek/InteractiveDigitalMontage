# InteractiveDigitalMontage
A Qt GUI program implementing [Interactive Digital Montage](http://grail.cs.washington.edu/projects/photomontage/).<br>
The program implements parts of the original paper, including:
- Data Penalty
  - Designated image
- Interactive Penalty
  - "colors" with a-expansion
  - "colors & gradients" with a-expansion (**the best**)
  - "colors & edges"  with a-b-swap
- Gradient Domain Fusion
  - By Eigen3 (fast and worked)
  - By MySolver (**slow and not-worked**, hope you can fix it)
 
 The Qt part applys QThread, which doesn't block the GUI while processing images, but also adds complexity to the algorithm implementation (intrusive design).<br>
 Though you cannot run the pure algorithm part (**MontageCore.h and MontageCore.cpp**) without Qt, you can still focus on the algorithm only by just reading codes or transferring the codes.

## Start up

* install Visual Studio
* install Qt 5
* install Visual Studio Qt Extention in Extention Management of Visual Studio
* open .sln with Visual Studio
