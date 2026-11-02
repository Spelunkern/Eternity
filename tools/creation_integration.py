"""Exercise character creation/loading in a disposable copy of the original SQL contracts."""
import secrets
import re
import subprocess

from database import identifier, literal, sql


def run(command, build_directory="build"):
    name = "Eternity_Test_Creation_" + secrets.token_hex(6)
    catalog = identifier(name)
    collation = sql("SELECT CONVERT(varchar(128),DATABASEPROPERTYEX('PS_GameData','Collation'));")
    if not re.fullmatch(r"[A-Za-z0-9_]+", collation):
        raise ValueError("Invalid source database collation identifier")
    sql(f"CREATE DATABASE {catalog} COLLATE {collation};")
    try:
        tables = ("Chars", "CharItems", "UserMaxGrow", "GuildChars", "_DeletedChars", "UserStoredMoney", "CharEvents",
                  "UserStoredItems", "CharSkills", "CharApplySkills", "CharQuests", "CharQuickSlots", "CharSavePoint",
                  "PointLog", "UserStoredPointItems", "MarketCharConcern", "FriendChars", "BanChars", "PointGiftNotify")
        for table in tables:
            sql(f"SELECT TOP (0) * INTO dbo.{identifier(table)} FROM PS_GameData.dbo.{identifier(table)};", name)
        sql("SELECT TOP (0) * INTO dbo.Users_Master FROM PS_UserData.dbo.Users_Master;", name)
        for source_database, source_tables in (("PS_GameData", tables), ("PS_UserData", ("Users_Master",))):
            names = ",".join(literal(table) for table in source_tables)
            defaults = sql("""SELECT t.name AS [table],c.name AS [column],d.definition
            FROM sys.default_constraints d JOIN sys.tables t ON t.object_id=d.parent_object_id
            JOIN sys.columns c ON c.object_id=t.object_id AND c.column_id=d.parent_column_id
            WHERE t.name IN (""" + names + ") FOR JSON PATH;", source_database, json_output=True)
            for default in defaults:
                sql(f"ALTER TABLE dbo.{identifier(default['table'])} ADD DEFAULT {default['definition']} "
                    f"FOR {identifier(default['column'])};", name)
        procedures = ("usp_Create_Char_R", "usp_Save_Char_Item_Add_E", "usp_Save_Char_Country_E", "usp_Read_Chars_R",
                      "usp_Read_Char_Items_Simple_R", "usp_Read_User_MaxGrow_R", "usp_Delete_Char_E", "usp_Read_Chars_Detail2_R",
                      "usp_Read_Char_Items_Detail_R", "usp_Read_User_StoredItems_Detail_R", "usp_Read_Char_Skills_R",
                      "usp_Read_Char_ApplySkills_R", "usp_Read_Char_Quest_R", "usp_Read_Char_FinishedQuest_R",
                      "usp_Read_Char_QuickSlots_R", "usp_Read_Char_SavePoint", "usp_Read_Chars_PointLog_R",
                      "usp_Read_User_StoredPointItems", "usp_Read_User_CashPoint_NCash", "usp_Read_Char_GuildJoinDate_R",
                      "usp_Read_Char_ConcernMarket_R", "usp_Read_FriendChar_R", "usp_Read_BanChar_R", "usp_Read_User_GiftPointItemNotify",
                      "usp_Save_Char_ApplySkill_Del_E", "usp_Save_Char_ApplySkill_Add_E2")
        names = ",".join(literal(procedure) for procedure in procedures)
        modules = sql("""SELECT p.name,m.definition FROM sys.sql_modules m
            JOIN sys.procedures p ON p.object_id=m.object_id
            WHERE p.name IN (""" + names + ") FOR JSON PATH;", "PS_GameData", json_output=True)
        if len(modules) != len(procedures):
            raise ValueError("Original character procedure set is incomplete")
        for module in modules:
            definition = module["definition"]
            if module["name"].lower() == "usp_read_user_cashpoint_ncash":
                dependency = "PS_UserData.dbo.Users_Master"
                if dependency not in definition:
                    raise ValueError("Original cash procedure dependency changed")
                # Only the fixture redirects its cross-database account reference to the scratch account table.
                definition = definition.replace(dependency, "dbo.Users_Master")
            sql(f"EXEC({literal(definition)});", name)
        # A failure on the second starter item proves the outer character transaction rolls back.
        sql("ALTER TABLE dbo.CharItems ADD CONSTRAINT EternityRejectFixtureItem CHECK(ItemID<>2147483000); "
            "ALTER TABLE dbo.CharApplySkills ADD CONSTRAINT EternityRejectFixtureEffect CHECK(SkillID<>32760); "
            "CREATE USER [eternity] FOR LOGIN [eternity]; "
            "GRANT SELECT,INSERT,UPDATE,DELETE,EXECUTE ON SCHEMA::dbo TO [eternity];", name)
        subprocess.run(command + [f"{build_directory}/creation-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/creation-network-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/players-sql-integration", name], check=True)
    finally:
        # Only the fresh, randomly named fixture catalog is removed.
        sql(f"ALTER DATABASE {catalog} SET SINGLE_USER WITH ROLLBACK IMMEDIATE; DROP DATABASE {catalog};")
