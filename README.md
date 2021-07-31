# InteractiveDigitalMontage
A Qt GUI program implementing [Interactive Digital Montage](http://grail.cs.washington.edu/projects/photomontage/)<br>
The program implements parts of the original paper, including:
- Data Penalty
  - Designated image
- Interactive Penalty
  - "colors" with a-expansion
  - "colors & gradients" with a-expansion (the best)
  - "colors & edges"  with a-b-swap
- Gradient Domain Fusion
  - By Eigen3 (fast and worked)
  - By MySolver (Slow and not-worked)

## Start up

* install Visual Studio
* install Qt 5
* install Visual Studio Qt Extention in Extention Management of Visual Studio
* open .sln with Visual Studio
