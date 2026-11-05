"""Validate original guild procedures in a disposable catalog, preserving originals."""
import secrets
import subprocess
from database import sql,identifier,literal


def run(command,build_directory='build'):
    name='Eternity_Test_Guild_'+secrets.token_hex(6)
    sql(f'CREATE DATABASE {identifier(name)};')
    try:
        for table in ('Guilds','GuildDetails','GuildChars','Chars','GuildNpcLv','GuildStoredItems'):
            sql(f'SELECT TOP (0) * INTO dbo.{identifier(table)} FROM PS_GameData.dbo.{identifier(table)};',name)
        # The recovered procedure orders by G.Rank, absent from recovered Guilds.
        # Supply its expected column only in this disposable fixture.
        sql('ALTER TABLE dbo.Guilds ADD Rank int NOT NULL DEFAULT 0;',name)
        for procedure in ('usp_Read_MaxGuildID_R','usp_Read_Guild2_R','usp_Read_GuildChar_R','usp_Read_GuildNpcLv_R','usp_Read_Guild_StoredItems_Detail_R','usp_Save_Guild_StoredItem_Add_E','usp_Save_Guild_StoredItem_Mod_E','usp_Save_Guild_StoredItem_Del_E','usp_Save_Guild_NpcLv_Add_E','usp_Save_Guild_NpcLv_Mod_E','usp_Save_GuildInfo_E2','usp_Save_GuildChar_E','usp_Create_GuildChar_E','usp_Delete_Guild_E'):
            definition=sql('SELECT OBJECT_DEFINITION(OBJECT_ID('+literal('dbo.'+procedure)+')) AS definition FOR JSON PATH;','PS_GameData',json_output=True)[0]['definition']
            sql("GO\n"+definition,name)
        sql("INSERT dbo.Guilds(GuildID,GuildName,MasterUserID,MasterCharID,MasterName,Country,TotalCount,GuildPoint,Del,CreateDate,Rank) VALUES(10,'NativeGuild','native',1000,'Master',1,63,100,0,GETDATE(),3),(20,'EmptyGuild','native',2000,'Empty',0,0,200,0,GETDATE(),2),(30,'Deleted','native',3000,'Gone',0,0,0,1,GETDATE(),1); INSERT dbo.GuildDetails(GuildID,UseHouse,BuyHouse,Rank,Etin,EtinReturnCnt,KeepEtin,Remark) VALUES(10,1,1,2,300,7,9999,'Hello');",name)
        sql("INSERT dbo.GuildNpcLv(GuildID,GNpcType,NpcLevel,Number,CreateTime,Del) VALUES(10,21,3,0,GETDATE(),0),(10,-1,255,7,GETDATE(),0),(10,22,2,8,GETDATE(),0),(10,23,1,1,GETDATE(),1);",name)
        sql("SET IDENTITY_INSERT dbo.GuildStoredItems ON; INSERT dbo.GuildStoredItems(RowID,GuildID,ItemID,Type,TypeID,ItemUID,Slot,Quality,Gem1,Gem2,Gem3,Gem4,Gem5,Gem6,Craftname,[Count],MakeTime,Maketype,Del) VALUES(1,10,5001,5,1,1099511627777,4,-1,1,2,3,4,5,6,'01020304050607080903',7,'2024-05-10T12:13:14','M',0),(2,10,5001,5,1,1099511627778,4,123,6,5,4,3,2,1,' 01 02 ',2,'2024-05-10T12:13:13','M',0),(3,10,5001,5,1,1099511627779,1,124,0,0,0,0,0,0,NULL,0,'2024-05-10T12:13:12','M',0),(4,10,5001,5,1,1099511627780,240,125,0,0,0,0,0,0,'',1,'2024-05-10T12:13:11','M',0),(5,10,5001,5,1,1099511627781,2,126,0,0,0,0,0,0,'',1,'2024-05-10T12:13:10','M',1); SET IDENTITY_INSERT dbo.GuildStoredItems OFF;",name)
        # SELECT INTO omits default constraints; preserve the original insertion default.
        for table in ('GuildStoredItems','GuildNpcLv','GuildChars'):
            defaults=sql("SELECT c.name,d.definition FROM PS_GameData.sys.columns c JOIN PS_GameData.sys.default_constraints d ON c.default_object_id=d.object_id WHERE c.object_id=OBJECT_ID("+literal('PS_GameData.dbo.'+table)+") FOR JSON PATH;",name,json_output=True)
            for default in defaults:
                sql('ALTER TABLE dbo.'+identifier(table)+' ADD DEFAULT '+default['definition']+' FOR '+identifier(default['name'])+';',name)
        columns=sql("SELECT c.name,TYPE_NAME(c.user_type_id) AS type,c.is_nullable FROM sys.columns c WHERE c.object_id=OBJECT_ID('dbo.Chars') ORDER BY c.column_id FOR JSON PATH;",name,json_output=True)
        names=','.join(identifier(c['name']) for c in columns)
        statements=['SET IDENTITY_INSERT dbo.Chars ON;']
        for i in range(63):
            overrides={'CharID':str(1000+i),'CharName':literal('Master' if i==0 else 'NewMaster' if i==1 else 'Hero'+str(i)),'Level':'30','Job':'5','UserID':literal('native'),'UserUID':'2147483000','ServerID':'255'}
            values=[overrides.get(c['name'],'NULL' if c['is_nullable'] else 'GETDATE()' if c['type'] in ('datetime','smalldatetime') else "''" if 'char' in c['type'] else '0') for c in columns]
            statements.append(f'INSERT dbo.Chars({names}) VALUES('+','.join(values)+');')
            rank=2 if i==0 else 1 if i==1 else 0 if i>=61 else 3
            statements.append(f'INSERT dbo.GuildChars(GuildID,CharID,GuildLevel,Del,JoinDate) VALUES(10,{1000+i},{rank},0,GETDATE());')
        statements.append('SET IDENTITY_INSERT dbo.Chars OFF;')
        sql('\n'.join(statements),name)
        subprocess.run(command+[f'{build_directory}/guild-sql-integration',name],check=True)
    finally:
        sql(f'ALTER DATABASE {identifier(name)} SET SINGLE_USER WITH ROLLBACK IMMEDIATE; DROP DATABASE {identifier(name)};')
