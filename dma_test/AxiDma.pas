{$MODE DELPHIUNICODE}
unit AxiDma;

interface 

uses
  SysUtils, Classes, Unix, BaseUnix;

const
  MAP_SIZE = 4096;
  MAP_MASK = MAP_SIZE - 1;
  // Channel 1 (Memory Map To Stream) offsets
  MM2S_DMACR  = $00000000; // Channel 1 control register
  MM2S_DMASR  = $00000004; // Channel 1 status register (read half word only)
  MM2S_SA     = $00000018; // Channel 1 Memory Map to stream source address
  MM2S_LENGTH = $00000028; // Channel 1 Memory Map To Stream  Source Address (Address of incoming data)

  // Channel 2 (Stream  to memory map) offsets
  S2MM_DMACR  = $00000030; // Channel 2 control register
  S2MM_DMASR  = $00000034; // Channel 2 status register (read half word only)
  S2MM_DA     = $00000048; // Channel 2 Memory Map to stream destination address
  S2MM_LENGTH = $00000058; // Channel 1 Memory Map To Stream  Source Address (Address of incoming data)

type
  TByteArray = array of Byte;  
  TChannel = (cMM2S, cS2MM);  
  TAxiDma = class
  protected
    FBaseAddress: LongWord;
    FFileDesriptor: Integer;
    FMapBase: Pointer;
    function OpenDevMem: Boolean;
    procedure CloseDevMem;
  public
    constructor Create(ABaseAddress: LongWord);
    destructor Destroy; override;
    function GetControlResgister(Channel: TChannel): LongWord;
  end;

implementation

constructor TAxiDma.Create(ABaseAddress: LongWord);
begin
  FBaseAddress := ABaseAddress;
  FFileDesriptor := -1;
  FMapBase := nil;
end;

destructor TAxiDma.Destroy;
begin
  CloseDevMem;
  inherited Destroy;
end;

function TAxiDma.OpenDevMem: Boolean;
begin
  FFileDesriptor := fpOpen('/dev/mem', O_RDWR or O_SYNC);
end;

procedure TAxiDma.CloseDevMem;
begin
  if (FFileDesriptor > 0) then
    fpClose(FFileDesriptor);
end;

function TAxiDma.GetControlResgister(Channel: TChannel): LongWord;
var 
  LPhysicalAddress: LongWord;
  LVirtualAddress: Pointer;
begin
  Result := 0;
  case Channel of
  cMM2S: LPhysicalAddress := FBaseAddress +  MM2S_DMACR;
  cS2MM: LPhysicalAddress := FBaseAddress +  S2MM_DMACR;
  end;
  
  
  FMapBase := fpMMap(nil, MAP_SIZE, PROT_READ or PROT_WRITE, MAP_SHARED, FFileDesriptor, LPhysicalAddress and not MAP_MASK);
  if (FMapBase = nil) then
  begin
    WriteLn('fpMMap faile with error ', GetLastOSError);
    EXIT;
  end;

  try
    LVirtualAddress := FMapBase + (LPhysicalAddress and MAP_MASK);
    Result := (PLongWord(LVirtualAddress))^
  finally
    fpMUnMap(FMapBase, MAP_SIZE);
    FMapBase := nil;
  end;
end;

begin
end.