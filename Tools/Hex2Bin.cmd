-Disable_Sequence_Warnings      #disable warning about hex records not in ascending order
./Objects/CY2311_R2.hex         #input file name
-Intel                          #read input file in intel hex format
-fill 0xFF 0x0000 0x2000        #fill gaps from 0x0000-0x1FFF (8K) with value 0xFF
-Output_Block_Size=16           #generate hex records with 16 byte data length
-address-length=2               #generate 16-bit address records
-o ./Objects/CY2311_R2.bin      #output file name
-Binary                         #