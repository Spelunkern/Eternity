"""Validate original guild procedures in a disposable catalog, preserving originals."""
import secrets
import subprocess
from database import sql,identifier,literal


def run(command,build_directory='build'):
    name='Eternity_Test_Guild_'+secrets.token_hex(6)
    sql(f'CREATE DATABASE {identifier(name)};')
    try:
        for table in ('Guilds','GuildDetails','GuildChars','Chars'):
            sql(f'SELECT TOP (0) * INTO dbo.{identifier(table)} FROM PS_GameData.dbo.{identifier(table)};',name)
        # The recovered procedure orders by G.Rank, absent from recovered Guilds.
        # Supply its expected column only in this disposable fixture.
        sql('ALTER TABLE dbo.Guilds ADD Rank int NOT NULL DEFAULT 0;',name)
        for procedure in ('usp_Read_MaxGuildID_R','usp_Read_Guild2_R','usp_Read_GuildChar_R'):
            definition=sql('SELECT OBJECT_DEFINITION(OBJECT_ID('+literal('dbo.'+procedure)+')) AS definition FOR JSON PATH;','PS_GameData',json_output=True)[0]['definition']
            sql("GO\n"+definition,name)
        sql("INSERT dbo.Guilds(GuildID,GuildName,MasterUserID,MasterCharID,MasterName,Country,TotalCount,GuildPoint,Del,CreateDate,Rank) VALUES(10,'NativeGuild','native',1000,'Master',1,63,100,0,GETDATE(),3),(20,'EmptyGuild','native',2000,'Empty',0,0,200,0,GETDATE(),2),(30,'Deleted','native',3000,'Gone',0,0,0,1,GETDATE(),1); INSERT dbo.GuildDetails(GuildID,UseHouse,BuyHouse,Rank,Etin,EtinReturnCnt,KeepEtin,Remark) VALUES(10,1,1,2,300,7,9999,'Hello');",name)
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
